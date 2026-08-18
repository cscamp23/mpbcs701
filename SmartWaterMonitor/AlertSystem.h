#ifndef ALERT_SYSTEM_H
#define ALERT_SYSTEM_H

#include <Arduino.h>

class AlertSystem {
private:
    uint8_t buzzerPin;
    uint8_t ledPin;
    bool isAlertActive;

public:
    AlertSystem(uint8_t bPin, uint8_t lPin);
    void begin();
    
    // Checks sensor values against thresholds and triggers alerts if unsafe
    void checkConditions(float turbidity, float ph, float temperature);
    
    void triggerAlert();
    void clearAlert();
};

#endif // ALERT_SYSTEM_H
