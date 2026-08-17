#ifndef CONFIG_H
#define CONFIG_H

// --- Pin Definitions ---
// DHT11 Sensor
#define DHTPIN 2
#define DHTTYPE DHT11

// MQ135 Sensor
#define MQ135_PIN A0

// Local Alerts
#define BUZZER_PIN 8
#define LED_PIN 9

// ESP8266 SoftwareSerial Pins
#define ESP_RX 10
#define ESP_TX 11

// --- Thresholds ---
#define TEMP_THRESHOLD_C 35.0  // Alert if temp > 35 C
#define AQI_THRESHOLD   400    // Alert if analog reading > 400

// --- Wi-Fi & Cloud (ThingSpeak) ---
const char WIFI_SSID[] = "YOUR_WIFI_SSID";
const char WIFI_PASS[] = "YOUR_WIFI_PASSWORD";
const char THINGSPEAK_API_KEY[] = "YOUR_THINGSPEAK_API_KEY";
const char THINGSPEAK_IP[] = "184.106.153.149"; // api.thingspeak.com

// --- Timing ---
#define SENSOR_READ_INTERVAL 2000    // Read sensors every 2 seconds
#define CLOUD_UPLOAD_INTERVAL 15000  // Upload to ThingSpeak every 15 seconds (ThingSpeak limit is 15s)

#endif
