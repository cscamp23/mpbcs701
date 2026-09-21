"""
Virtual nodes — run the whole system before a single ESP32 arrives.

    python simulate.py                  # HTTP ingest into a local server
    python simulate.py --mqtt           # publish to the broker instead

Rooms follow their timetable with realistic slippage: classes start late, a few
get cancelled, study halls fill up in the evening, and one node is deliberately
left offline so you can see how the dashboard handles a dead sensor.
"""

from __future__ import annotations

import argparse
import json
import random
import time
import urllib.request
from datetime import datetime, time as dtime, timedelta, timezone
from pathlib import Path

import yaml

IST = timezone(timedelta(hours=5, minutes=30))
ROOMS_FILE = Path(__file__).resolve().parent / "rooms.yaml"
DEAD_NODE = "B102"          # unplugged on purpose


def hhmm(value: str) -> dtime:
    hh, mm = value.split(":")
    return dtime(int(hh), int(mm))


class VirtualRoom:
    def __init__(self, room: dict, slots: list[dict]) -> None:
        self.id = room["id"]
        self.kind = room.get("kind", "classroom")
        self.slots = slots
        self.state = "vacant"
        self.since = datetime.now(IST)
        self.cancelled: set[str] = set()

    def scheduled_now(self, now: datetime) -> dict | None:
        for slot in self.slots:
            if now.weekday() not in slot["days"]:
                continue
            start = datetime.combine(now.date(), hhmm(slot["start"]), tzinfo=IST)
            end = datetime.combine(now.date(), hhmm(slot["end"]), tzinfo=IST)
            # Students drift in a few minutes late and leave a touch early.
            if start + timedelta(minutes=3) <= now < end - timedelta(minutes=2):
                return slot
        return None

    def want(self, now: datetime) -> str:
        slot = self.scheduled_now(now)
        if slot:
            key = f"{now.date()}:{slot['start']}:{self.id}"
            if key not in self.cancelled and random.random() < 0.08:
                self.cancelled.add(key)          # ~8% of classes do not happen
            return "vacant" if key in self.cancelled else "occupied"

        if self.kind == "study":                 # reading halls have a life of their own
            hour = now.hour
            busy = 0.85 if 9 <= hour < 21 else 0.1
            return "occupied" if random.random() < busy else "vacant"

        if self.kind == "lab" and 9 <= now.hour < 18:
            return "occupied" if random.random() < 0.25 else "vacant"

        return "occupied" if random.random() < 0.05 else "vacant"

    def tick(self, now: datetime) -> dict:
        target = self.want(now)
        if target != self.state:
            self.state = target
            self.since = now
        idle = 0 if self.state == "occupied" else int((now - self.since).total_seconds())
        return {
            "room": self.id,
            "state": self.state,
            "change": idle == 0,
            "target": 3 if self.state == "occupied" else 0,
            "moving_cm": random.randint(80, 600) if self.state == "occupied" else 0,
            "static_cm": random.randint(80, 600) if self.state == "occupied" else 0,
            "energy": random.randint(45, 95) if self.state == "occupied" else random.randint(0, 8),
            "idle_s": idle,
            "rssi": random.randint(-75, -45),
            "uptime_s": int(time.time()) % 900000,
            "fw": "1.0.0-sim",
        }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://localhost:8000/api/ingest")
    ap.add_argument("--token", default="dev-token")
    ap.add_argument("--interval", type=float, default=10.0, help="seconds between rounds")
    ap.add_argument("--speed", type=float, default=1.0, help=">1 fast-forwards the clock")
    ap.add_argument("--mqtt", action="store_true")
    ap.add_argument("--broker", default="localhost")
    args = ap.parse_args()

    raw = yaml.safe_load(ROOMS_FILE.read_text())
    slots: dict[str, list[dict]] = {}
    for slot in raw.get("timetable", []):
        slots.setdefault(slot["room"], []).append(slot)

    rooms = [VirtualRoom(r, slots.get(r["id"], [])) for r in raw["rooms"]]

    publish = None
    if args.mqtt:
        import paho.mqtt.client as m
        client = m.Client(m.CallbackAPIVersion.VERSION2, client_id="roomstat-sim")
        client.username_pw_set("roomstat", "change-me-too")
        client.connect(args.broker, 1883, 60)
        client.loop_start()
        publish = lambda p: client.publish(f"roomstat/{p['room']}/state", json.dumps(p), retain=True)
    else:
        def publish(p: dict) -> None:
            req = urllib.request.Request(
                args.url,
                data=json.dumps(p).encode(),
                headers={"Content-Type": "application/json",
                         "Authorization": f"Bearer {args.token}"},
            )
            urllib.request.urlopen(req, timeout=5).read()

    virtual_now = datetime.now(IST)
    print(f"simulating {len(rooms)} rooms, {DEAD_NODE} left offline — ctrl-c to stop")

    while True:
        line = []
        for room in rooms:
            if room.id == DEAD_NODE:
                continue
            payload = room.tick(virtual_now)
            try:
                publish(payload)
            except Exception as exc:
                print(f"  {room.id}: {exc}")
            line.append(f"{room.id}:{'#' if payload['state'] == 'occupied' else '.'}")

        print(f"{virtual_now:%a %H:%M}  " + " ".join(line))
        virtual_now += timedelta(seconds=args.interval * args.speed)
        time.sleep(args.interval)


if __name__ == "__main__":
    main()
