# WOOF – IoT Sensors & Firmware

This repository contains the firmware and hardware configuration for the **IoT sensor modules** used in the WOOF system.  
The firmware runs on two separate hardware units:
1. **Maduino Zero 4G LTE** – Responsible for GPS location tracking and sending data via LTE or USB.
2. **ESP32 + HX711** – Responsible for food and water weight measurement and sending readings via Wi-Fi.

---

## Hardware Components

### **1. Maduino Zero 4G LTE (SIM7600E + GPS)**
- **Purpose**: Tracks dog's location in real time.
- **Built-in Modules**:
  - **SIM7600E 4G LTE** for cellular data transmission.
  - **GPS Receiver** for live positioning.
- **External Components**:
  - LTE Antenna
  - GPS Antenna
- **Usage in WOOF**:
  - Sends location data every few seconds to **Firebase Realtime Database**.
  - Works even without Wi-Fi by using a SIM card.

---

### **2. ESP32 + HX711 Weight Sensor**
- **Purpose**: Measures food and water intake via smart bowls.
- **Components**:
  - **HX711 Amplifier** connected to a load cell for weight measurement.
  - **ESP32** for reading sensor data and sending it to the cloud.
- **Usage in WOOF**:
  - Monitors feeding patterns and hydration.
  - Sends readings via Wi-Fi to Firebase for real-time updates.

---

## Data Flow

```plaintext
[HX711 + ESP32] ---> Wi-Fi ---> Firebase Realtime Database
[Maduino Zero GPS] ---> LTE/USB ---> Firebase Realtime Database

---

