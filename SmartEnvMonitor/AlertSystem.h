#ifndef ALERT_SYSTEM_H
#define ALERT_SYSTEM_H

#include <Arduino.h>
#include "SensorManager.h"

void initAlerts();
void checkAlerts(const SensorData& data);

#endif
