#ifndef CONFIG_H
#define CONFIG_H

// ==========================================
// PIN DEFINITIONS
// ==========================================

// Sensors
#define PIN_TURBIDITY A0     // Analog pin for Turbidity Sensor
#define PIN_PH A1            // Analog pin for pH Sensor
#define PIN_TEMP 2           // Digital pin for DS18B20 Temperature Sensor (Requires 4.7k pull-up)

// Alerts
#define PIN_BUZZER 8         // Digital pin for Active Buzzer
#define PIN_LED 9            // Digital pin for Warning LED

// ESP8266 Software Serial
#define ESP_RX 10            // Connect to ESP8266 TX
#define ESP_TX 11            // Connect to ESP8266 RX (via voltage divider)

// ==========================================
// SAFETY THRESHOLDS
// ==========================================
// Modify these based on the specific water source you are monitoring.

// Turbidity (Typically 0 to 3000 NTU. Higher voltage means cleaner water for many modules)
// For analog read (0-1023), mapping will be done in the sensor class. 
// A simple threshold: if voltage drops too low, it's too turbid.
#define THRESHOLD_TURBIDITY_MAX 50.0 // Example NTU limit

// pH Levels (Safe drinking water is usually 6.5 to 8.5)
#define THRESHOLD_PH_MIN 6.5
#define THRESHOLD_PH_MAX 8.5

// Temperature (Safe range in Celsius)
#define THRESHOLD_TEMP_MAX 35.0

// ==========================================
// WI-FI & IOT CLOUD CREDENTIALS (THINGSPEAK)
// ==========================================
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
#define THINGSPEAK_API_KEY "YOUR_THINGSPEAK_API_KEY"

#endif // CONFIG_H
