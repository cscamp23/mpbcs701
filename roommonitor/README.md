# RoomStat — live free-classroom board for a college campus

One sensor per classroom. A student opens a link and sees which rooms are empty
right now, and how long they'll stay empty before the next class.

This is the campus IoT project that actually gets used, because every student
already has the problem: a free hour, nowhere to sit, and four floors of doors
to try. It also produces something the admin block wants — real room
utilisation numbers instead of a timetable that says 100%.

```
  classroom                      server (any old PC / Pi)          phone
 ┌──────────────┐   MQTT/wifi   ┌──────────────────────────┐      ┌────────┐
 │ ESP32        │──────────────▶│ mosquitto  →  app.py     │◀─────│ board  │
 │ + LD2410C    │  every 60s    │            →  SQLite     │ HTTP │        │
 │ (mmWave)     │  + on change  │            →  /api/rooms │      └────────┘
 └──────────────┘               └──────────────────────────┘
```

## Why mmWave and not a camera or wifi sniffing

This matters more than the electronics — it's what gets the project approved.

- **No camera.** A 60 GHz/24 GHz radar sees "a body is moving/breathing at 3.2 m".
  It cannot see who. Nothing identifiable ever leaves the room.
- **No MAC addresses.** Counting phones via wifi probes is the obvious student
  hack, it tracks individuals, and modern MAC randomisation breaks it anyway.
- **Detects stillness.** PIR misses a room full of people writing an exam.
  The LD2410 reports stationary targets, which is exactly the failure case
  a classroom sensor has to survive.

Put that paragraph in your report. It's the part faculty will ask about.

## Bill of materials, per room

| Part | Why | Approx ₹ |
|---|---|---|
| ESP32 DevKit V1 | wifi + two UARTs + OTA updates | 350 |
| HLK-LD2410C mmWave module | presence, including stationary people | 400 |
| 5 V 1 A USB adapter + cable | rooms have sockets; no batteries to change | 130 |
| Small ABS enclosure + screws | survives a cleaning crew | 90 |
| Dupont wires | 4 connections | 30 |
| | **per room** | **≈ ₹1,000** |

Server: any machine already on the campus LAN. A Raspberry Pi 4 or a retired
lab desktop handles hundreds of nodes — the write volume is one row per room
per minute.

A 16-room pilot is about ₹16,000. Start with 4 rooms (₹4,000) and one floor.

## Wiring

```
LD2410C        ESP32
  VCC  ───────── 5V        (module needs 5 V; its IO is 3.3 V safe)
  GND  ───────── GND
  TX   ───────── GPIO16
  RX   ───────── GPIO17
  OUT  ───────── GPIO18     hardware presence line, used if the UART goes quiet
```

Mount 1.8–2.2 m high on the wall behind the teaching position, pointing into
the room. Not above the door — you'll catch corridor traffic. Not facing a
window or a ceiling fan; moving blades and swaying curtains read as motion.

## Flash a node

1. Arduino IDE → Boards Manager → install **esp32** by Espressif.
2. Library Manager → **PubSubClient**, **ArduinoJson** (v7).
3. Open `firmware/roomstat_node.ino`, edit `firmware/config.h`:
   set `ROOM_ID` to the room (must match an id in `server/rooms.yaml`), and
   fill in wifi + broker details.
4. Upload over USB once. After that, every update goes over the air —
   `ArduinoOTA` means you never carry a laptop up a ladder twice.

The onboard LED blinks slowly when the room reads vacant and stays solid when
occupied, so you can verify a node from the doorway.

### If campus wifi is WPA2-Enterprise

`WiFi.begin(ssid, pass)` will not associate with an 802.1X network. In order of
preference: (1) ask IT for an IoT SSID with a plain PSK — this is a routine
request and they will usually prefer it to student devices on the staff VLAN;
(2) use the `esp_wifi_sta_wpa2_ent_set_identity/username/password` APIs from
`esp_wpa2.h`; (3) put a cheap travel router in the block and uplink it.

## Run the server

```bash
sudo apt install mosquitto mosquitto-clients        # broker
sudo mosquitto_passwd -c /etc/mosquitto/passwd roomstat

cd server
pip install -r requirements.txt
export MQTT_HOST=localhost MQTT_PASS='...' ROOMSTAT_TOKEN='...'
python app.py                                       # serves :8000
```

`server/rooms.yaml` is the only file you edit each semester: the room list and
the timetable copied off the department noticeboard. Everything else derives
from it.

### Before any hardware arrives

```bash
python simulate.py --speed 60
```

Virtual nodes follow the real timetable, cancel about 8% of classes, fill the
reading hall in the evening, and leave B102 unplugged so you can see how a dead
sensor is handled. Open `http://localhost:8000` and the board is live. This is
also your demo if a node dies the morning of the viva.

## API

| Endpoint | Use |
|---|---|
| `GET /api/rooms` | current state of every room, with timetable overlay |
| `GET /api/rooms/{id}/history?hours=12` | 15-minute occupancy buckets |
| `GET /api/stats?days=7` | utilisation % per room — the admin-facing number |
| `POST /api/ingest` | HTTP fallback for nodes that can't reach the broker |

## The logic that makes it trustworthy

A naive build publishes raw sensor state and the board flickers all day.
Three things fix that:

- **Asymmetric debounce.** Occupied is confirmed in 4 seconds; vacant needs
  3 continuous minutes. Someone stepping out to take a call must not free the
  room, but a room filling up should go red immediately.
- **Heartbeat + last will.** A node reports every 60 s even when nothing
  changes, and registers an MQTT last-will. The server marks a room `offline`
  after 3.5 missed beats, so "empty" and "broken" never look the same.
- **Timetable overlay.** Sensor truth beats the schedule. A room booked for a
  class that nobody showed up to is shown as *free*, with the caveat that it
  was booked. That single feature is the one students will tell each other
  about.

## Rollout that actually happens

1. **Week 1 — one node, your own classroom.** Run it for five days. Log every
   false reading against what you actually saw. Tune `VACATE_CONFIRM_MS` and
   the LD2410 sensitivity gates.
2. **Week 2 — one floor, 4–5 nodes.** Confirm wifi coverage in the far corners
   before you buy more. This is where projects die; check it early.
3. **Week 3 — share the link with one class.** Watch what they ask for. The
   first request is always "add the library".
4. **Then take `GET /api/stats` to the HoD.** Utilisation data is what buys you
   a purchase order for the rest of the block.

## Things that will go wrong, and the fix

| Symptom | Cause | Fix |
|---|---|---|
| Room never goes vacant | ceiling fan, curtain, or corridor through a doorway in view | re-aim the module, raise the LD2410 stationary gate for that distance bin |
| Flickers between states | sensitivity too high at the far gates | lower gate energy thresholds; raise `OCCUPY_CONFIRM_MS` |
| Node drops off wifi nightly | AP kicks idle clients | `WiFi.setSleep(false)` is already set; also give the node a DHCP reservation |
| Everything offline at once | broker restarted | nodes reconnect on their own; check `mosquitto` is enabled at boot |

## Where to take it next

- **Occupancy count**, not just presence: a second LD2410 at the door with
  in/out direction gives an approximate headcount.
- **Energy**: a relay or IR blaster that cuts fans and lights after 10 minutes
  vacant. This is where the project stops costing money and starts saving it,
  and it is the strongest line in a report.
- **Bookings**: let a student hold an empty room for 30 minutes from the board.
- **Prediction**: with a month of history, `GET /api/stats` data is enough to
  answer "will A203 be free at 3pm on Thursday" with plain logistic regression.
