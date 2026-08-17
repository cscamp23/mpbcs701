#include "SensorManager.h"
#include "config.h"
#include <DHT.h>

DHT dht(DHTPIN, DHTTYPE);

void initSensors() {
    dht.begin();
    pinMode(MQ135_PIN, INPUT);
}

SensorData readSensors() {
    SensorData data;
    
    // Read Temperature and Humidity
    data.humidity = dht.readHumidity();
    data.temperature = dht.readTemperature();
    
    // Read Air Quality (Raw analog value for simplicity)
    data.airQuality = analogRead(MQ135_PIN);

    // Check if any reads failed
    if (isnan(data.humidity) || isnan(data.temperature)) {
        data.isValid = false;
        data.temperature = 0.0;
        data.humidity = 0.0;
    } else {
        data.isValid = true;
    }

    return data;
}
