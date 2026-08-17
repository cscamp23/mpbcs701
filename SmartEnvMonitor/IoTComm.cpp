#include "IoTComm.h"
#include "config.h"
#include <SoftwareSerial.h>

SoftwareSerial espSerial(ESP_RX, ESP_TX); // RX, TX

// Helper function to send AT command and wait for response
String sendAT(String command, const int timeout) {
    String response = "";
    espSerial.print(command + "\r\n");
    long int time = millis();
    while ((time + timeout) > millis()) {
        while (espSerial.available()) {
            char c = espSerial.read();
            response += c;
        }
    }
    return response;
}

void initIoT() {
    espSerial.begin(9600); // Default baud rate for most ESP-01 modules
    delay(1000);
    
    // Reset module
    sendAT("AT+RST", 2000);
    
    // Set to Station mode
    sendAT("AT+CWMODE=1", 1000);
    
    // Connect to WiFi
    String cmd = "AT+CWJAP=\"";
    cmd += WIFI_SSID;
    cmd += "\",\"";
    cmd += WIFI_PASS;
    cmd += "\"";
    sendAT(cmd, 5000);
}

bool sendDataToCloud(const SensorData& data) {
    if (!data.isValid) return false;

    // Open TCP connection to ThingSpeak
    String cmd = "AT+CIPSTART=\"TCP\",\"";
    cmd += THINGSPEAK_IP;
    cmd += "\",80";
    String response = sendAT(cmd, 2000);
    
    if (response.indexOf("Error") != -1 || response.indexOf("ERROR") != -1) {
        return false;
    }

    // Prepare GET request string
    String getStr = "GET /update?api_key=";
    getStr += THINGSPEAK_API_KEY;
    getStr += "&field1=";
    getStr += String(data.temperature);
    getStr += "&field2=";
    getStr += String(data.humidity);
    getStr += "&field3=";
    getStr += String(data.airQuality);
    getStr += "\r\n\r\n";

    // Send data length
    cmd = "AT+CIPSEND=";
    cmd += String(getStr.length());
    sendAT(cmd, 1000);

    // Send the actual GET request
    response = sendAT(getStr, 2000);
    
    // Close connection (ThingSpeak usually closes it, but good practice)
    sendAT("AT+CIPCLOSE", 1000);
    
    return response.indexOf("SEND OK") != -1;
}
