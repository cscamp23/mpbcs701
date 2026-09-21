"""
RoomStat server — ingests classroom occupancy from ESP32 nodes and serves the
student-facing dashboard.

    pip install -r requirements.txt
    python app.py

Runs on :8000. Point the dashboard at it, point the nodes at the broker.
"""

from __future__ import annotations

import json
import os
import sqlite3
import threading
from contextlib import contextmanager
from datetime import datetime, time as dtime, timedelta, timezone
from pathlib import Path
from typing import Any, Iterator

import yaml
from fastapi import FastAPI, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse

BASE = Path(__file__).resolve().parent
DB_PATH = Path(os.getenv("ROOMSTAT_DB", BASE / "roomstat.db"))
ROOMS_FILE = Path(os.getenv("ROOMSTAT_ROOMS", BASE / "rooms.yaml"))
DASHBOARD = BASE.parent / "dashboard" / "index.html"

MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_USER = os.getenv("MQTT_USER", "roomstat")
MQTT_PASS = os.getenv("MQTT_PASS", "change-me-too")
MQTT_PREFIX = os.getenv("MQTT_PREFIX", "roomstat")

INGEST_TOKEN = os.getenv("ROOMSTAT_TOKEN", "dev-token")
OFFLINE_AFTER = timedelta(seconds=210)   # 3.5 missed heartbeats
IST = timezone(timedelta(hours=5, minutes=30))

_db_lock = threading.Lock()


# --------------------------------------------------------------------- storage
@contextmanager
def db() -> Iterator[sqlite3.Connection]:
    conn = sqlite3.connect(DB_PATH, timeout=10)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    try:
        yield conn
        conn.commit()
    finally:
        conn.close()


SCHEMA = """
CREATE TABLE IF NOT EXISTS readings (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    room      TEXT NOT NULL,
    ts        TEXT NOT NULL,
    state     TEXT NOT NULL,
    changed   INTEGER NOT NULL DEFAULT 0,
    idle_s    INTEGER,
    energy    INTEGER,
    rssi      INTEGER,
    uptime_s  INTEGER
);
CREATE INDEX IF NOT EXISTS ix_readings_room_ts ON readings(room, ts);

CREATE TABLE IF NOT EXISTS live (
    room       TEXT PRIMARY KEY,
    state      TEXT NOT NULL,
    since      TEXT NOT NULL,
    last_seen  TEXT NOT NULL,
    idle_s     INTEGER,
    rssi       INTEGER,
    uptime_s   INTEGER,
    fw         TEXT,
    link       TEXT NOT NULL DEFAULT 'online'
);
"""


def init_db() -> None:
    with db() as conn:
        conn.executescript(SCHEMA)


# ------------------------------------------------------------------- registry
def load_registry() -> dict[str, Any]:
    with ROOMS_FILE.open() as fh:
        raw = yaml.safe_load(fh)

    rooms = {r["id"]: r for r in raw.get("rooms", [])}
    slots: dict[str, list[dict[str, Any]]] = {rid: [] for rid in rooms}
    for slot in raw.get("timetable", []):
        if slot["room"] in slots:
            slots[slot["room"]].append(slot)
    for entries in slots.values():
        entries.sort(key=lambda s: s["start"])

    return {"rooms": rooms, "timetable": slots, "campus": raw.get("campus", {})}


REGISTRY = load_registry()


def _parse_hhmm(value: str) -> dtime:
    hh, mm = value.split(":")
    return dtime(int(hh), int(mm))


def schedule_for(room_id: str, now: datetime) -> dict[str, Any]:
    """What the timetable *claims* about this room right now."""
    weekday = now.weekday()  # Mon=0
    today = [s for s in REGISTRY["timetable"].get(room_id, []) if weekday in s["days"]]

    current, upcoming = None, None
    for slot in today:
        start = datetime.combine(now.date(), _parse_hhmm(slot["start"]), tzinfo=IST)
        end = datetime.combine(now.date(), _parse_hhmm(slot["end"]), tzinfo=IST)
        if start <= now < end:
            current = {**slot, "ends_at": end.isoformat()}
        elif start > now and upcoming is None:
            upcoming = {**slot, "starts_at": start.isoformat(),
                        "starts_in_min": int((start - now).total_seconds() // 60)}

    return {"current": current, "next": upcoming}


# --------------------------------------------------------------------- ingest
def record(payload: dict[str, Any]) -> None:
    room = payload.get("room")
    if not room:
        raise ValueError("payload has no room")
    if room not in REGISTRY["rooms"]:
        raise ValueError(f"unknown room {room!r}")

    state = payload.get("state", "unknown")
    if state not in {"occupied", "vacant", "unknown"}:
        raise ValueError(f"bad state {state!r}")

    now = datetime.now(IST).isoformat()

    with _db_lock, db() as conn:
        conn.execute(
            "INSERT INTO readings (room, ts, state, changed, idle_s, energy, rssi, uptime_s)"
            " VALUES (?,?,?,?,?,?,?,?)",
            (room, now, state, int(bool(payload.get("change"))),
             payload.get("idle_s"), payload.get("energy"),
             payload.get("rssi"), payload.get("uptime_s")),
        )

        prev = conn.execute("SELECT state, since FROM live WHERE room=?", (room,)).fetchone()
        since = prev["since"] if prev and prev["state"] == state else now

        conn.execute(
            "INSERT INTO live (room, state, since, last_seen, idle_s, rssi, uptime_s, fw, link)"
            " VALUES (?,?,?,?,?,?,?,?,'online')"
            " ON CONFLICT(room) DO UPDATE SET"
            "   state=excluded.state, since=excluded.since, last_seen=excluded.last_seen,"
            "   idle_s=excluded.idle_s, rssi=excluded.rssi, uptime_s=excluded.uptime_s,"
            "   fw=excluded.fw, link='online'",
            (room, state, since, now, payload.get("idle_s"),
             payload.get("rssi"), payload.get("uptime_s"), payload.get("fw")),
        )


def mark_link(room: str, link: str) -> None:
    if room not in REGISTRY["rooms"]:
        return
    now = datetime.now(IST).isoformat()
    with _db_lock, db() as conn:
        conn.execute(
            "INSERT INTO live (room, state, since, last_seen, link)"
            " VALUES (?, 'unknown', ?, ?, ?)"
            " ON CONFLICT(room) DO UPDATE SET link=excluded.link",
            (room, now, now, link),
        )


# ----------------------------------------------------------------------- mqtt
def start_mqtt() -> None:
    try:
        import paho.mqtt.client as mqtt_client
    except ImportError:
        print("[mqtt] paho-mqtt not installed — HTTP ingest only")
        return

    def on_connect(client, _userdata, _flags, reason_code, _props=None):
        print(f"[mqtt] connected rc={reason_code}")
        client.subscribe([(f"{MQTT_PREFIX}/+/state", 0), (f"{MQTT_PREFIX}/+/status", 0)])

    def on_message(_client, _userdata, msg):
        parts = msg.topic.split("/")
        if len(parts) != 3:
            return
        _, room, leaf = parts
        try:
            if leaf == "status":
                mark_link(room, msg.payload.decode().strip() or "offline")
            else:
                record(json.loads(msg.payload.decode()))
        except Exception as exc:                      # a bad node must not kill the loop
            print(f"[mqtt] dropped {msg.topic}: {exc}")

    try:
        client = mqtt_client.Client(
            mqtt_client.CallbackAPIVersion.VERSION2, client_id="roomstat-server"
        )
    except AttributeError:                            # paho 1.x
        client = mqtt_client.Client(client_id="roomstat-server")

    client.username_pw_set(MQTT_USER, MQTT_PASS)
    client.on_connect = on_connect
    client.on_message = on_message
    client.will_set(f"{MQTT_PREFIX}/server/status", "offline", retain=True)

    def run():
        while True:
            try:
                client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
                client.loop_forever()
            except Exception as exc:
                print(f"[mqtt] {exc} — retrying in 5s")
                threading.Event().wait(5)

    threading.Thread(target=run, daemon=True, name="mqtt").start()


# ------------------------------------------------------------------------ api
app = FastAPI(title="RoomStat", version="1.0.0")
app.add_middleware(
    CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"]
)


@app.on_event("startup")
def _startup() -> None:
    init_db()
    start_mqtt()
    print(f"[roomstat] {len(REGISTRY['rooms'])} rooms registered")


def room_view(room: dict[str, Any], live: sqlite3.Row | None, now: datetime) -> dict[str, Any]:
    sched = schedule_for(room["id"], now)

    if live is None:
        state, link, since, last_seen = "unknown", "never-seen", None, None
    else:
        last_seen = datetime.fromisoformat(live["last_seen"])
        stale = now - last_seen > OFFLINE_AFTER
        link = "offline" if (stale or live["link"] != "online") else "online"
        state = live["state"] if link == "online" else "unknown"
        since, last_seen = live["since"], live["last_seen"]

    free_for = None
    if state == "vacant":
        if sched["current"]:
            # Timetabled, but nobody is in it — free until the next block starts.
            nxt = sched["next"]
            free_for = nxt["starts_in_min"] if nxt else 120
        elif sched["next"]:
            free_for = sched["next"]["starts_in_min"]
        else:
            free_for = 120                      # nothing scheduled for the rest of the day

    return {
        "id": room["id"],
        "name": room.get("name", room["id"]),
        "block": room.get("block", "—"),
        "floor": room.get("floor", 0),
        "capacity": room.get("capacity"),
        "kind": room.get("kind", "classroom"),
        "state": state,
        "link": link,
        "since": since,
        "last_seen": last_seen,
        "idle_s": live["idle_s"] if live else None,
        "rssi": live["rssi"] if live else None,
        "scheduled": sched["current"],
        "next_class": sched["next"],
        "free_for_min": free_for,
    }


@app.get("/api/rooms")
def api_rooms() -> dict[str, Any]:
    now = datetime.now(IST)
    with db() as conn:
        live = {r["room"]: r for r in conn.execute("SELECT * FROM live")}

    rooms = [room_view(r, live.get(rid), now) for rid, r in REGISTRY["rooms"].items()]
    rooms.sort(key=lambda r: (r["block"], -r["floor"], r["id"]))

    return {
        "as_of": now.isoformat(),
        "campus": REGISTRY["campus"],
        "free": sum(1 for r in rooms if r["state"] == "vacant"),
        "total": len(rooms),
        "rooms": rooms,
    }


@app.get("/api/rooms/{room_id}/history")
def api_history(room_id: str, hours: int = 12) -> dict[str, Any]:
    if room_id not in REGISTRY["rooms"]:
        raise HTTPException(404, "unknown room")
    hours = max(1, min(hours, 168))

    now = datetime.now(IST)
    since = (now - timedelta(hours=hours)).isoformat()

    with db() as conn:
        rows = conn.execute(
            "SELECT ts, state FROM readings WHERE room=? AND ts>=? ORDER BY ts",
            (room_id, since),
        ).fetchall()

    # Fold into 15-minute buckets: a bucket is busy if anything in it was occupied.
    buckets: dict[str, str] = {}
    for row in rows:
        ts = datetime.fromisoformat(row["ts"])
        key = ts.replace(minute=(ts.minute // 15) * 15, second=0, microsecond=0).isoformat()
        if buckets.get(key) != "occupied":
            buckets[key] = row["state"]

    return {
        "room": room_id,
        "hours": hours,
        "buckets": [{"t": k, "state": v} for k, v in sorted(buckets.items())],
    }


@app.get("/api/stats")
def api_stats(days: int = 7) -> dict[str, Any]:
    """Utilisation per room — the number the admin block actually cares about."""
    days = max(1, min(days, 90))
    since = (datetime.now(IST) - timedelta(days=days)).isoformat()

    with db() as conn:
        rows = conn.execute(
            "SELECT room,"
            "       SUM(state='occupied') AS busy,"
            "       COUNT(*)              AS total"
            " FROM readings WHERE ts>=? GROUP BY room",
            (since,),
        ).fetchall()

    return {
        "days": days,
        "rooms": [
            {
                "id": r["room"],
                "name": REGISTRY["rooms"].get(r["room"], {}).get("name", r["room"]),
                "samples": r["total"],
                "utilisation_pct": round(100 * r["busy"] / r["total"], 1) if r["total"] else 0.0,
            }
            for r in sorted(rows, key=lambda x: x["room"])
        ],
    }


@app.post("/api/ingest")
async def api_ingest(request: Request) -> JSONResponse:
    """HTTP path for nodes that cannot reach the broker, and for testing."""
    if request.headers.get("authorization") != f"Bearer {INGEST_TOKEN}":
        raise HTTPException(401, "bad token")
    try:
        record(await request.json())
    except ValueError as exc:
        raise HTTPException(400, str(exc)) from exc
    return JSONResponse({"ok": True})


@app.get("/")
def dashboard() -> FileResponse:
    if not DASHBOARD.exists():
        raise HTTPException(404, "dashboard/index.html is missing")
    return FileResponse(DASHBOARD)


if __name__ == "__main__":
    import uvicorn

    uvicorn.run(app, host="0.0.0.0", port=int(os.getenv("PORT", "8000")))
