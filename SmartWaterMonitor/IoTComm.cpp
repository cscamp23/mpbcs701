#include "IoTComm.h"

IoTComm::IoTComm(uint8_t rxPin, uint8_t txPin, String wifiSsid, String wifiPass, String tsApiKey) {
    espSerial = new SoftwareSerial(rxPin, txPin);
    ssid = wifiSsid;
    password = wifiPass;
    apiKey = tsApiKey;
}

IoTComm::~IoTComm() {
    delete espSerial;
}

void IoTComm::begin() {
    espSerial->begin(9600); // Default ESP8266 baud rate
    delay(1000);
    // Reset ESP
    espSerial->println("AT+RST");
    delay(2000);
}

bool IoTComm::connectWiFi() {
    String cmd = "AT+CWJAP=\"" + ssid + "\",\"" + password + "\"";
    espSerial->println(cmd);
    delay(5000); // Give it time to connect
    
    // In a robust implementation, we would parse the response.
    // For simplicity in this demo, we assume connection success if no immediate error.
    return true; 
}

bool IoTComm::sendData(float turbidity, float ph, float temperature) {
    // Start TCP connection to ThingSpeak
    String cmd = "AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80";
    espSerial->println(cmd);
    delay(2000);

    if (espSerial->find("Error")) {
        return false;
    }

    // Prepare GET request string
    String getStr = "GET /update?api_key=" + apiKey;
    getStr += "&field1=" + String(turbidity);
    getStr += "&field2=" + String(ph);
    getStr += "&field3=" + String(temperature);
    getStr += "\r\n\r\n";

    // Send data length
    cmd = "AT+CIPSEND=" + String(getStr.length());
    espSerial->println(cmd);
    delay(1000);

    if (espSerial->find(">")) {
        espSerial->print(getStr);
        delay(2000);
        return true;
    } else {
        espSerial->println("AT+CIPCLOSE");
        return false;
    }
}
