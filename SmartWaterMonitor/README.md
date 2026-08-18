# Smart Water Quality & Safety Monitor

An Internet of Things (IoT) mini-project built with an Arduino Uno that continuously monitors water quality using Turbidity, pH, and Temperature sensors. It provides local alerts via a buzzer and LED when water quality crosses safety thresholds, and pushes live data to a ThingSpeak cloud dashboard using an ESP8266 Wi-Fi module.

## Features
*   **Real-time Monitoring:** Tracks water clarity (Turbidity), acidity/alkalinity (pH), and temperature (DS18B20).
*   **Local Alerts:** Visual (LED) and auditory (Buzzer) warnings if the water becomes unsafe.
*   **Cloud Dashboard:** Uploads sensor telemetry to ThingSpeak every 15 seconds.
*   **Modular Codebase:** Clean C++ structure separating sensor logic, alerts, IoT communication, and configuration.

## Recommended Hardware
To build this project, you will need the following components:
1.  **Arduino Uno R3** (The main microcontroller)
2.  **ESP8266 (ESP-01) Wi-Fi Module** (For internet connectivity)
3.  **Analog Turbidity Sensor** (Measures water clarity)
4.  **Analog pH Sensor Kit** (Includes probe and signal conditioning board)
5.  **DS18B20 Temperature Sensor** (Ensure it is the **waterproof** stainless steel probe version)
6.  **5V Active Buzzer**
7.  **LED & 220Ω Resistor** (For visual alert)
8.  **4.7kΩ Resistor** (Pull-up resistor required for the DS18B20 data line)
9.  **1kΩ and 2kΩ Resistors** (For ESP8266 voltage divider to protect its 3.3V RX pin from the Arduino's 5V TX)
10. Breadboard and Jumper Wires

## Circuit / Wiring Guide

> [!WARNING] 
> The ESP8266 operates strictly on **3.3V logic**. Do NOT connect its VCC or RX pins directly to 5V Arduino pins. Use the 3.3V pin for power, and a voltage divider for the RX line.

### Sensors & Alerts
| Component | Arduino Pin / Connection | Notes |
| :--- | :--- | :--- |
| **Turbidity Sensor (A0)** | Analog Pin `A0` | Power with 5V. |
| **pH Sensor (Po)** | Analog Pin `A1` | Power with 5V. |
| **DS18B20 (Data)** | Digital Pin `2` | Power with 5V. **Important:** Connect a 4.7kΩ resistor between Data and 5V. |
| **Buzzer (+)** | Digital Pin `8` | Ground the other pin. |
| **LED (Anode)** | Digital Pin `9` | Connect via a 220Ω resistor. Ground cathode. |

### ESP8266 (ESP-01)
| ESP8266 Pin | Arduino Connection |
| :--- | :--- |
| **VCC** | 3.3V |
| **CH_PD (EN)** | 3.3V |
| **GND** | GND |
| **TX** | Arduino Pin `10` (SoftwareSerial RX) |
| **RX** | Arduino Pin `11` (SoftwareSerial TX) *via Voltage Divider* |

*(Voltage Divider for Arduino TX -> ESP RX: Arduino Pin 11 -> 1kΩ Resistor -> ESP RX -> 2kΩ Resistor -> GND)*

## Setup & Assembly Instructions

### 1. Cloud Setup (ThingSpeak)
1. Sign up for a free account at [ThingSpeak.com](https://thingspeak.com/).
2. Create a **New Channel**.
3. Enable 3 fields: `Field 1` (Turbidity), `Field 2` (pH), and `Field 3` (Temperature).
4. Save the channel and navigate to the **API Keys** tab. Copy your **Write API Key**.

### 2. Software Configuration & Assembly
1.  **Assemble the Hardware:** Carefully wire the components on a breadboard following the tables above. Double-check the 4.7k resistor on the DS18B20 and the voltage divider on the ESP8266.
2.  **Open the Project:** Open the `SmartWaterMonitor.ino` file in the Arduino IDE. The IDE should automatically open the other `.h` and `.cpp` files as tabs.
3.  **Update Credentials:** Go to the `config.h` tab and update your Wi-Fi name, Wi-Fi password, and the ThingSpeak API Key you copied earlier.
4.  **Install Libraries:** Go to **Sketch > Include Library > Manage Libraries**. Search for and install:
    *   `OneWire` (by Paul Stoffregen)
    *   `DallasTemperature` (by Miles Burton)
    *   *Note: SoftwareSerial is built-in.*

### 3. Uploading the Code
1.  **Disconnect the ESP8266 TX/RX pins** (Pins 10 and 11) temporarily. This prevents interference during the upload process.
2.  Select **Arduino Uno** and the correct COM port in **Tools**.
3.  Click **Upload**.
4.  Once the upload is complete, **reconnect the TX/RX pins**.

### 4. Testing & Calibration
1.  Open the **Serial Monitor** in the Arduino IDE (set to 9600 baud).
2.  You should see the system initialize, connect to Wi-Fi, and begin reading sensor values.
3.  **Turbidity Test:** Place the turbidity sensor in clear water, then in muddy water. The NTU value should change, and if it crosses the threshold set in `config.h`, the buzzer and LED should trigger.
4.  **pH Calibration:** pH sensors usually require calibration. Measure a known solution (e.g., pH 7 buffer). If the Serial Monitor output is incorrect, adjust the `phCalibrationValue` variable inside `WaterSensors.cpp` and re-upload.
5.  **Cloud Verification:** Log in to ThingSpeak. You should see new data points appearing on your graphs every 15 seconds.

## Troubleshooting
*   **Sensor Readings are 0 or fluctuating wildly:** Check all ground (GND) connections. Ensure the 4.7k resistor is properly seated for the temperature sensor.
*   **Cannot connect to Wi-Fi:** Verify your SSID and password in `config.h`. Ensure your Wi-Fi router is broadcasting a 2.4GHz network (ESP8266 does not support 5GHz).
*   **ThingSpeak not updating:** Check the Serial Monitor to see if the ESP8266 is reporting an error. Ensure the TX/RX pins are correctly wired (Arduino TX to ESP RX, and vice-versa) and the voltage divider is correct.
