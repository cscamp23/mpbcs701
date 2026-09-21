#pragma once

// ---- the only line you change per classroom -------------------------------
// Must match an id in server/rooms.yaml. Keep it short: it doubles as the
// MQTT client id, the OTA hostname and the DHCP hostname.
#define ROOM_ID      "A201"

// ---- campus network --------------------------------------------------------
// If your college wifi is WPA2-Enterprise (802.1X), plain WiFi.begin() will not
// work. Two options, in order of preference:
//   1. Ask IT for a device/IoT SSID with a PSK — this is a normal request.
//   2. Use WiFi.begin() with esp_wifi_sta_wpa2_ent_* APIs. See docs/DEPLOY.md.
#define WIFI_SSID    "CAMPUS-IOT"
#define WIFI_PASS    "change-me"

// ---- broker ----------------------------------------------------------------
#define MQTT_HOST    "10.0.12.40"      // the machine running server/app.py
#define MQTT_PORT    1883
#define MQTT_USER    "roomstat"
#define MQTT_PASS    "change-me-too"
#define MQTT_PREFIX  "roomstat"        // topics: roomstat/<ROOM_ID>/state

// ---- maintenance -----------------------------------------------------------
#define OTA_PASS     "change-me-three"
#define FW_VERSION   "1.0.0"
