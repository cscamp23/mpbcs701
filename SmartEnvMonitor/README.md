# IoT-Based Smart Environmental Monitoring and Alert System

An Internet of Things (IoT) project built with Arduino Uno that continuously monitors ambient temperature, humidity, and air quality. It triggers local alerts via a buzzer and LED when readings cross defined safety thresholds and pushes live data to a ThingSpeak cloud dashboard via an ESP8266 Wi-Fi module.

## Features
*   **Real-time Monitoring:** Tracks temperature and humidity (DHT11) and air quality (MQ135).
*   **Local Alerts:** Visual (LED) and auditory (Buzzer) warnings if thresholds are exceeded.
*   **Cloud Dashboard:** Uploads sensor telemetry to ThingSpeak every 15 seconds.
*   **Modular Codebase:** Clean, multi-file C++ structure separating sensor logic, alerts, IoT communication, and configuration.

## Hardware Required
*   Arduino Uno
*   ESP8266 (ESP-01) Wi-Fi Module
*   DHT11 Temperature & Humidity Sensor
*   MQ135 Air Quality Sensor
*   5V Active Buzzer
*   LED & 220Ω Resistor
*   1kΩ and 2kΩ Resistors (for ESP8266 Voltage Divider)
*   Breadboard and Jumper Wires

## Project Structure
```text
SmartEnvMonitor/
├── SmartEnvMonitor.ino   # Main sketch: setup() and loop() orchestrating the modules
├── config.h              # Global pin definitions, thresholds, and Wi-Fi/Cloud credentials
├── SensorManager.h/.cpp  # Initialization and data reading for DHT11 and MQ135
├── AlertSystem.h/.cpp    # Logic to evaluate thresholds and trigger Buzzer/LED
└── IoTComm.h/.cpp        # SoftwareSerial AT commands to ESP8266 for pushing data to ThingSpeak
```

## Circuit / Wiring Guide

> **Warning:** The ESP8266 operates on **3.3V logic**. Do not connect its VCC or RX pins directly to 5V Arduino pins without a voltage divider or regulator.

### Sensors & Alerts
| Component | Arduino Pin |
| :--- | :--- |
| DHT11 Data | Digital Pin 2 |
| MQ135 A0 | Analog Pin A0 |
| Buzzer (+) | Digital Pin 8 |
| LED (Anode) | Digital Pin 9 (via 220Ω resistor) |

### ESP8266 (ESP-01)
| ESP8266 Pin | Connection |
| :--- | :--- |
| VCC | 3.3V |
| CH_PD (EN) | 3.3V |
| GND | GND |
| TX | Arduino Pin 10 (SoftwareSerial RX) |
| RX | Arduino Pin 11 (SoftwareSerial TX) *via Voltage Divider* |

*(Voltage Divider for Arduino TX -> ESP RX: Arduino Pin 11 -> 1kΩ Resistor -> ESP RX -> 2kΩ Resistor -> GND)*

## Setup Instructions

### 1. Cloud Setup (ThingSpeak)
1. Sign up for a free account at [ThingSpeak.com](https://thingspeak.com/).
2. Create a **New Channel**.
3. Enable 3 fields: `Field 1` (Temperature), `Field 2` (Humidity), and `Field 3` (Air Quality).
4. Save the channel and navigate to the **API Keys** tab. Copy the **Write API Key**.

### 2. Software Configuration
1. Open the project in the Arduino IDE.
2. Open `config.h` and update your network and cloud credentials:
   ```cpp
   const char WIFI_SSID[] = "YOUR_WIFI_SSID";
   const char WIFI_PASS[] = "YOUR_WIFI_PASSWORD";
   const char THINGSPEAK_API_KEY[] = "YOUR_THINGSPEAK_API_KEY";
   ```
3. Install dependencies: Go to **Sketch > Include Library > Manage Libraries** and search for `DHT sensor library` by Adafruit. Install it along with the Adafruit Unified Sensor dependency.

### 3. Upload and Run
1. Disconnect the TX/RX pins to the ESP8266 while uploading the sketch to the Arduino (to avoid serial interference).
2. Select **Arduino Uno** and the correct COM port in the IDE.
3. Click **Upload**.
4. Reconnect the TX/RX pins.
5. Open the **Serial Monitor** (set to 9600 baud) to view real-time readings and confirm successful Wi-Fi connection and ThingSpeak data upload.
