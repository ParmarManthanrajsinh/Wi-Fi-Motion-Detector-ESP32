# Wi-Fi Motion Detector (ESP32)

This project implements a **Wi-Fi-based Motion Detector** using an ESP32 microcontroller. Instead of relying on traditional PIR sensors or cameras, this system detects physical movement by monitoring fluctuations and sudden drops in the Wi-Fi signal strength (RSSI) from the connected Access Point. 

It is designed to be monitored continuously via a serial dashboard and can be dynamically configured over a USB serial connection without needing to reflash the device.

## Features

- **RSSI-Based Motion Detection:** Uses the Received Signal Strength Indicator (RSSI) of the connected Wi-Fi network to detect motion.
- **Dynamic Baseline Calibration:** Implements an Exponential Moving Average (EMA) to establish a baseline RSSI and filter out gradual changes.
- **Serial JSON Output:** Streams real-time data (`rssi`, `baseline`, `motion` state, `calibrating` status) over serial in JSON format for easy parsing by a dashboard.
- **Dynamic Wi-Fi Configuration:** Listens for Wi-Fi credentials over the serial connection (`{"ssid": "...", "password": "..."}`) and securely saves them to Non-Volatile Storage (NVS).
- **Hardware Feedback:** Triggers the built-in LED (GPIO 2) and an external output pin (GPIO 19) when motion is detected.

## Hardware Required

- An ESP32 development board (e.g., ESP32-DevKitC)
- A USB cable for power and serial communication 
- (Optional) LED or Relay connected to GPIO 19 for external motion indication

## How it Works

1. **Initialization & Wi-Fi Connection:** The device attempts to connect to the saved Wi-Fi network from NVS. If no credentials exist or the connection fails, it waits for credentials via serial input.
2. **Calibration:** Upon connection, the ESP32 samples the RSSI for a few seconds to establish an initial baseline.
3. **Monitoring:** The device continuously monitors the current RSSI. The baseline is updated using an Exponential Moving Average.
4. **Motion Detection:** If the current RSSI drops continuously below the baseline by a predefined threshold (e.g., 6.0 dB), motion is triggered, updating the JSON output and hardware pins.

## Getting Started

### Prerequisites

- [ESP-IDF (Espressif IoT Development Framework)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) installed and configured on your machine.

### Build and Flash

1. Open your terminal and activate the ESP-IDF environment (`get_idf`).
2. Set the target to your board (default is ESP32):
   ```bash
   idf.py set-target esp32
   ```
3. Build the project, flash it, and open the monitor:
   ```bash
   idf.py build flash monitor
   ```

### Connecting to Wi-Fi

If the ESP32 does not have stored credentials or fails to connect, it will output `{"error": "Not Connected"}`. 
You can dynamically configure the Wi-Fi by sending a JSON payload via the serial console (ensure no newline characters break the JSON format):

```json
{"ssid": "Your_WiFi_SSID", "password": "Your_WiFi_Password"}
```
The ESP32 will receive the credentials, save them to NVS, and automatically reboot to connect.

## Serial Output Format

When connected, the device outputs a JSON string every 200ms containing the system's state:

```json
{"rssi": -55, "baseline": -52.45, "motion": true, "calibrating": false}
```

This output can be easily ingested by a Python script, a web dashboard (like the included `dashboard.html`), or Node-RED for visualization and further home automation integration.

## License

This project is open-source and based on the Espressif IoT Development Framework (ESP-IDF).
