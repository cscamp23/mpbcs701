#include "WaterSensors.h"

WaterSensors::WaterSensors(uint8_t tPin, uint8_t pPin, uint8_t tmpPin) {
    turbidityPin = tPin;
    phPin = pPin;
    tempPin = tmpPin;
    phCalibrationValue = 21.34; // This needs calibration for accurate pH readings

    oneWire = new OneWire(tempPin);
    sensors = new DallasTemperature(oneWire);
}

WaterSensors::~WaterSensors() {
    delete sensors;
    delete oneWire;
}

void WaterSensors::begin() {
    pinMode(turbidityPin, INPUT);
    pinMode(phPin, INPUT);
    sensors->begin();
}

float WaterSensors::readTurbidity() {
    int sensorValue = analogRead(turbidityPin);
    float voltage = sensorValue * (5.0 / 1024.0); 
    
    // Convert voltage to NTU (Nephelometric Turbidity Units)
    // This formula varies significantly by sensor. 
    // Typical generic formula:
    float turbidity = -1120.4 * square(voltage) + 5742.3 * voltage - 4352.9;
    
    // Bounds checking
    if (turbidity < 0) {
        turbidity = 0;
    }
    return turbidity;
}

float WaterSensors::readPH() {
    int sensorValue = analogRead(phPin);
    float voltage = sensorValue * (5.0 / 1024.0);
    
    // Calculate pH. Formula varies by sensor module.
    // Example formula: pH = 3.5 * voltage + offset
    float phValue = 3.5 * voltage + (phCalibrationValue - 21.34); 
    return phValue;
}

float WaterSensors::readTemperature() {
    sensors->requestTemperatures(); 
    float tempC = sensors->getTempCByIndex(0);
    return tempC;
}
