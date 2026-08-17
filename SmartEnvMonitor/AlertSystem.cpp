#include "AlertSystem.h"
#include "config.h"

void initAlerts() {
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
}

void checkAlerts(const SensorData& data) {
    if (!data.isValid) return;

    bool isTempHigh = data.temperature > TEMP_THRESHOLD_C;
    bool isAirPoor = data.airQuality > AQI_THRESHOLD;

    if (isTempHigh || isAirPoor) {
        digitalWrite(BUZZER_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);
    } else {
        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
    }
}
