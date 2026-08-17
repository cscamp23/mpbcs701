#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>

struct SensorData {
    float temperature;
    float humidity;
    int airQuality;
    bool isValid;
};

void initSensors();
SensorData readSensors();

#endif
