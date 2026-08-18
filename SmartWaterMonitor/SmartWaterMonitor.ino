#include "config.h"
#include "WaterSensors.h"
#include "AlertSystem.h"
#include "IoTComm.h"

// Instantiate modules
WaterSensors sensors(PIN_TURBIDITY, PIN_PH, PIN_TEMP);
AlertSystem alertSystem(PIN_BUZZER, PIN_LED);
IoTComm cloudComm(ESP_RX, ESP_TX, WIFI_SSID, WIFI_PASS, THINGSPEAK_API_KEY);

unsigned long lastUploadTime = 0;
const unsigned long UPLOAD_INTERVAL = 15000; // 15 seconds (ThingSpeak limit)

void setup() {
    // Initialize Serial Monitor
    Serial.begin(9600);
    Serial.println("Smart Water Quality Monitor initializing...");

    // Initialize modules
    sensors.begin();
    alertSystem.begin();
    cloudComm.begin();

    // Connect to Wi-Fi
    Serial.print("Connecting to Wi-Fi...");
    cloudComm.connectWiFi();
    Serial.println(" Done (or timeout).");
}

void loop() {
    // 1. Read Sensors
    float turbidity = sensors.readTurbidity();
    float ph = sensors.readPH();
    float temperature = sensors.readTemperature();

    // Print to Serial Monitor
    Serial.print("Turbidity (NTU): "); Serial.print(turbidity);
    Serial.print(" | pH: "); Serial.print(ph);
    Serial.print(" | Temp (C): "); Serial.println(temperature);

    // 2. Evaluate Alerts Locally
    alertSystem.checkConditions(turbidity, ph, temperature);

    // 3. Upload to Cloud (every 15 seconds)
    if (millis() - lastUploadTime > UPLOAD_INTERVAL) {
        Serial.println("Uploading to ThingSpeak...");
        bool success = cloudComm.sendData(turbidity, ph, temperature);
        if (success) {
            Serial.println("Upload successful.");
        } else {
            Serial.println("Upload failed.");
        }
        lastUploadTime = millis();
    }

    delay(2000); // Small delay for loop stability
}
