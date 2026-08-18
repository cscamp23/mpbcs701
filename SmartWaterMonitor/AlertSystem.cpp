#include "AlertSystem.h"
#include "config.h"

AlertSystem::AlertSystem(uint8_t bPin, uint8_t lPin) {
    buzzerPin = bPin;
    ledPin = lPin;
    isAlertActive = false;
}

void AlertSystem::begin() {
    pinMode(buzzerPin, OUTPUT);
    pinMode(ledPin, OUTPUT);
    clearAlert();
}

void AlertSystem::checkConditions(float turbidity, float ph, float temperature) {
    bool isUnsafe = false;

    // Check Turbidity
    if (turbidity > THRESHOLD_TURBIDITY_MAX) {
        isUnsafe = true;
    }

    // Check pH
    if (ph < THRESHOLD_PH_MIN || ph > THRESHOLD_PH_MAX) {
        isUnsafe = true;
    }

    // Check Temperature
    if (temperature > THRESHOLD_TEMP_MAX) {
        isUnsafe = true;
    }

    if (isUnsafe) {
        triggerAlert();
    } else {
        clearAlert();
    }
}

void AlertSystem::triggerAlert() {
    if (!isAlertActive) {
        digitalWrite(ledPin, HIGH);
        // Simple beep pattern
        tone(buzzerPin, 1000); 
        isAlertActive = true;
    }
}

void AlertSystem::clearAlert() {
    if (isAlertActive) {
        digitalWrite(ledPin, LOW);
        noTone(buzzerPin);
        isAlertActive = false;
    }
}
