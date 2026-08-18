#ifndef WATER_SENSORS_H
#define WATER_SENSORS_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

class WaterSensors {
private:
    uint8_t turbidityPin;
    uint8_t phPin;
    uint8_t tempPin;

    OneWire* oneWire;
    DallasTemperature* sensors;

    // Calibration values
    float phCalibrationValue; 

public:
    WaterSensors(uint8_t tPin, uint8_t pPin, uint8_t tmpPin);
    ~WaterSensors();

    void begin();
    
    float readTurbidity();
    float readPH();
    float readTemperature();
};

#endif // WATER_SENSORS_H
