#ifndef IOT_COMM_H
#define IOT_COMM_H

#include <Arduino.h>
#include <SoftwareSerial.h>

class IoTComm {
private:
    SoftwareSerial* espSerial;
    String ssid;
    String password;
    String apiKey;

    void sendCommand(String command, int maxTime, char readReplay[]);

public:
    IoTComm(uint8_t rxPin, uint8_t txPin, String wifiSsid, String wifiPass, String tsApiKey);
    ~IoTComm();

    void begin();
    bool connectWiFi();
    bool sendData(float turbidity, float ph, float temperature);
};

#endif // IOT_COMM_H
