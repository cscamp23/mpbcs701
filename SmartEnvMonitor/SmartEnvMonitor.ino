#include "config.h"
#include "SensorManager.h"
#include "AlertSystem.h"
#include "IoTComm.h"

unsigned long previousSensorMillis = 0;
unsigned long previousCloudMillis = 0;

void setup() {
    Serial.begin(9600);
    Serial.println("Starting Smart Environmental Monitor...");
    
    initSensors();
    initAlerts();
    initIoT();
    
    Serial.println("System Initialized.");
}

void loop() {
    unsigned long currentMillis = millis();
    
    // Read sensors and update alerts based on SENSOR_READ_INTERVAL
    if (currentMillis - previousSensorMillis >= SENSOR_READ_INTERVAL) {
        previousSensorMillis = currentMillis;
        
        SensorData data = readSensors();
        
        if (data.isValid) {
            Serial.print("Temp: "); Serial.print(data.temperature); Serial.print(" C, ");
            Serial.print("Hum: "); Serial.print(data.humidity); Serial.print(" %, ");
            Serial.print("AQI: "); Serial.println(data.airQuality);
            
            checkAlerts(data);
        } else {
            Serial.println("Failed to read from DHT sensor!");
        }
    }
    
    // Upload data to cloud based on CLOUD_UPLOAD_INTERVAL
    if (currentMillis - previousCloudMillis >= CLOUD_UPLOAD_INTERVAL) {
        previousCloudMillis = currentMillis;
        
        SensorData data = readSensors(); // Get freshest data for upload
        if (data.isValid) {
            Serial.println("Uploading data to ThingSpeak...");
            bool success = sendDataToCloud(data);
            if (success) {
                Serial.println("Upload successful.");
            } else {
                Serial.println("Upload failed.");
            }
        }
    }
}
