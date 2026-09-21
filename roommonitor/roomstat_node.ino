/*
 * RoomStat campus node — ESP32 + LD2410C mmWave presence sensor
 * -------------------------------------------------------------
 * One node per classroom. Mount above the whiteboard or on the rear wall,
 * 1.8-2.2 m high, facing into the room. Mains powered from a 5V USB adapter.
 *
 * Reports occupancy over MQTT. Publishes on state change (debounced) and as a
 * heartbeat every HEARTBEAT_MS so the server can tell "empty room" apart from
 * "dead sensor". Uses an MQTT last-will so an unplugged node marks itself
 * offline within one keepalive window.
 *
 * Board:  ESP32 Dev Module (esp32 core >= 2.0.11)
 * Libs:   PubSubClient (Nick O'Leary), ArduinoJson v7
 *
 * Wiring (LD2410C -> ESP32):
 *   VCC  -> 5V        (the module needs 5V; its IO is 3.3V safe)
 *   GND  -> GND
 *   TX   -> GPIO16    (RX2)
 *   RX   -> GPIO17    (TX2)   put a 1k series resistor here if you are fussy
 *   OUT  -> GPIO18    (hardware presence line, used as a sanity check)
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include "config.h"

// ---------------------------------------------------------------- pins / timing
static const int PIN_RADAR_RX = 16;   // ESP32 receives on this pin
static const int PIN_RADAR_TX = 17;
static const int PIN_RADAR_OUT = 18;
static const int PIN_LED = 2;

static const uint32_t HEARTBEAT_MS      = 60000;   // report even when nothing changes
static const uint32_t OCCUPY_CONFIRM_MS = 4000;    // presence must persist this long
static const uint32_t VACATE_CONFIRM_MS = 180000;  // 3 min of silence before "free"
static const uint32_t RADAR_STALE_MS    = 5000;    // no frames -> fall back to OUT pin

// ---------------------------------------------------------------- state
enum Occupancy : uint8_t { UNKNOWN = 0, VACANT = 1, OCCUPIED = 2 };

struct RadarReading {
  bool     presence     = false;
  uint8_t  target       = 0;    // 0 none, 1 moving, 2 stationary, 3 both
  uint16_t movingCm     = 0;
  uint8_t  movingEnergy = 0;
  uint16_t staticCm     = 0;
  uint8_t  staticEnergy = 0;
  uint32_t lastFrameMs  = 0;
};

void publishState(bool isChange);
void connectMqtt();
void connectWifi();

WiFiClient   net;
PubSubClient mqtt(net);
HardwareSerial radar(2);

RadarReading reading;
Occupancy    stableState    = UNKNOWN;
Occupancy    candidate      = UNKNOWN;
uint32_t     candidateSince = 0;
uint32_t     lastPublish    = 0;
uint32_t     lastPresenceMs = 0;
uint32_t     bootEpoch      = 0;

char topicState[96];
char topicStatus[96];

// ---------------------------------------------------------------- LD2410 parser
/*
 * Basic target frame:
 *   F4 F3 F2 F1 | len(2, LE) | 02 AA | state | movingCm(2) | movingEnergy
 *   | staticCm(2) | staticEnergy | detectCm(2) | 55 00 | F8 F7 F6 F5
 * We only keep the fields we actually use and ignore engineering-mode extras.
 */
void pumpRadar() {
  static uint8_t  buf[64];
  static uint8_t  idx = 0;
  static uint16_t want = 0;
  static uint8_t  hdr = 0;

  const uint8_t HEAD[4] = {0xF4, 0xF3, 0xF2, 0xF1};

  while (radar.available()) {
    uint8_t b = radar.read();

    if (hdr < 4) {                       // hunting for the frame header
      hdr = (b == HEAD[hdr]) ? hdr + 1 : (b == HEAD[0] ? 1 : 0);
      idx = 0;
      want = 0;
      continue;
    }

    if (want == 0) {                     // two length bytes, little endian
      buf[idx++] = b;
      if (idx == 2) {
        want = buf[0] | (buf[1] << 8);
        idx = 0;
        if (want == 0 || want > sizeof(buf)) { hdr = 0; want = 0; }  // bogus, resync
      }
      continue;
    }

    buf[idx++] = b;
    if (idx < want) continue;

    hdr = 0;                             // full payload captured
    want = 0;
    idx = 0;

    if (buf[0] != 0x02 || buf[1] != 0xAA) continue;   // not a target-data frame

    reading.target       = buf[2];
    reading.movingCm     = buf[3] | (buf[4] << 8);
    reading.movingEnergy = buf[5];
    reading.staticCm     = buf[6] | (buf[7] << 8);
    reading.staticEnergy = buf[8];
    reading.presence     = reading.target != 0;
    reading.lastFrameMs  = millis();
  }

  // If the UART goes quiet, trust the module's digital presence pin instead.
  if (millis() - reading.lastFrameMs > RADAR_STALE_MS) {
    reading.presence = digitalRead(PIN_RADAR_OUT) == HIGH;
    reading.target = reading.presence ? 2 : 0;
  }
}

// ---------------------------------------------------------------- occupancy logic
void updateOccupancy() {
  const uint32_t now = millis();
  if (reading.presence) lastPresenceMs = now;

  const Occupancy observed = reading.presence ? OCCUPIED : VACANT;

  if (observed != candidate) {
    candidate = observed;
    candidateSince = now;
    return;
  }

  const uint32_t held = now - candidateSince;
  const uint32_t need = (candidate == OCCUPIED) ? OCCUPY_CONFIRM_MS : VACATE_CONFIRM_MS;

  if (held >= need && stableState != candidate) {
    stableState = candidate;
    publishState(true);
  }
}

// ---------------------------------------------------------------- MQTT
const char* stateName(Occupancy s) {
  switch (s) {
    case OCCUPIED: return "occupied";
    case VACANT:   return "vacant";
    default:       return "unknown";
  }
}

void publishState(bool isChange) {
  if (!mqtt.connected()) return;

  JsonDocument doc;
  doc["room"]      = ROOM_ID;
  doc["state"]     = stateName(stableState);
  doc["change"]    = isChange;
  doc["target"]    = reading.target;
  doc["moving_cm"] = reading.movingCm;
  doc["static_cm"] = reading.staticCm;
  doc["energy"]    = max(reading.movingEnergy, reading.staticEnergy);
  doc["idle_s"]    = (millis() - lastPresenceMs) / 1000;
  doc["rssi"]      = WiFi.RSSI();
  doc["uptime_s"]  = millis() / 1000;
  doc["fw"]        = FW_VERSION;

  char payload[320];
  const size_t n = serializeJson(doc, payload);
  mqtt.publish(topicState, (const uint8_t*)payload, n, true);   // retained
  lastPublish = millis();
}

void connectMqtt() {
  while (!mqtt.connected()) {
    Serial.print("mqtt... ");
    const bool ok = mqtt.connect(
        ROOM_ID, MQTT_USER, MQTT_PASS,
        topicStatus, 1, true, "offline");        // last will

    if (ok) {
      Serial.println("up");
      mqtt.publish(topicStatus, "online", true);
      publishState(false);
      return;
    }
    Serial.printf("failed rc=%d\n", mqtt.state());
    delay(3000);
  }
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(ROOM_ID);
  WiFi.setSleep(false);                          // campus APs drop sleepy clients
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("wifi");
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    Serial.print(".");
    delay(400);
  }
  digitalWrite(PIN_LED, HIGH);
  Serial.printf(" %s\n", WiFi.localIP().toString().c_str());
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_RADAR_OUT, INPUT);

  radar.begin(256000, SERIAL_8N1, PIN_RADAR_RX, PIN_RADAR_TX);

  snprintf(topicState,  sizeof(topicState),  "%s/%s/state",  MQTT_PREFIX, ROOM_ID);
  snprintf(topicStatus, sizeof(topicStatus), "%s/%s/status", MQTT_PREFIX, ROOM_ID);

  connectWifi();

  ArduinoOTA.setHostname(ROOM_ID);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.begin();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(45);
  connectMqtt();

  lastPresenceMs = millis();
}

void loop() {
  ArduinoOTA.handle();

  if (WiFi.status() != WL_CONNECTED) connectWifi();
  if (!mqtt.connected()) connectMqtt();
  mqtt.loop();

  pumpRadar();
  updateOccupancy();

  if (millis() - lastPublish >= HEARTBEAT_MS) publishState(false);

  // Slow blink = vacant, solid = occupied. Handy when debugging on a ladder.
  digitalWrite(PIN_LED, stableState == OCCUPIED ? HIGH : (millis() / 1000) % 2);

  delay(20);
}
