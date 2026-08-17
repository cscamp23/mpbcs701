#ifndef IOT_COMM_H
#define IOT_COMM_H

#include <Arduino.h>
#include "SensorManager.h"

void initIoT();
bool sendDataToCloud(const SensorData& data);

#endif
