# waterFlow-Turbidity-Station
A comprehensive IoT water monitoring solution built with ESP32 that provides real-time measurement of water flow and turbidity with a stable WiFi hotspot interface.

## Features

### 🌊 Dual Sensor Monitoring
- **Water Flow Measurement**: YF-S201 Hall effect sensor on GPIO 4
- **Turbidity Detection**: Optical turbidity sensor on GPIO 36
- **Real-time Data Collection**: 500ms sampling interval
- **Raw & Processed Values**: Both voltage readings and calibrated measurements

### 📡 Stable WiFi Connectivity
- **Access Point Mode**: Creates stable WiFi hotspot (ESP32-SENSORS)
- **Captive Portal**: DNS server integration for seamless connection
- **WIFI_AP_STA Mode**: Enhanced stability with dual-mode operation
- **No Disconnection Issues**: Proven plantech WiFi implementation

### 📊 Web Dashboard
- **Real-time Display**: Live sensor readings with auto-refresh
- **Data Collection Controls**: Start/Stop data logging
- **Calibration Interface**: Interactive turbidity sensor calibration
- **Responsive Design**: Mobile-friendly web interface

### 💾 Data Export & Storage
- **Multiple Formats**: CSV, JSON, and TXT export options
- **SPIFFS Storage**: Local file system for web assets
- **Error-free Parsing**: Robust JSON handling with comprehensive error reporting
- **Memory Optimized**: Handles up to 1000 data points efficiently

## Hardware Requirements

- ESP32 DOIT DevKit V1
- YF-S201 Water Flow Sensor
- Turbidity Sensor (analog output)
- Jumper wires and breadboard
- Power supply (5V recommended)

## Pin Configuration

| Component | ESP32 Pin |
|-----------|-----------|
| Water Flow Sensor | GPIO 4 (Interrupt) |
| Turbidity Sensor | GPIO 36 (A0) |

## Network Configuration

- **SSID**: ESP32-SENSORS
- **Password**: abudojana
- **IP Address**: 192.168.10.10
- **Access**: Connect to WiFi and visit http://192.168.10.10

## Quick Start

1. **Hardware Setup**: Connect sensors to specified GPIO pins
2. **Upload Code**: Flash the sketch to ESP32 using Arduino IDE
3. **Connect**: Join the "ESP32-SENSORS" WiFi network
4. **Monitor**: Open web browser to 192.168.10.10
5. **Calibrate**: Use the calibration interface for accurate readings
6. **Collect Data**: Start monitoring and export results

## Technical Specifications

- **Microcontroller**: ESP32 (240MHz dual-core)
- **Flash Memory**: Uses SPIFFS file system
- **Libraries**: WiFi, WebServer, ArduinoJson, DNSServer
- **Data Rate**: Real-time collection every 500ms
- **Export Formats**: CSV, JSON, TXT with error handling
- **Memory Management**: Prevents overflow with 1000 point limit

## Use Cases

- Water quality research and monitoring
- Industrial flow measurement systems
- Environmental data collection
- IoT sensor network development
- Educational water analysis projects
- Thesis and research documentation

## Project Structure

```
sketch_aug12a/
├── sketch_aug12a.ino          # Main application code
├── plantech/                  # Reference WiFi implementation
├── README.md                  # Setup and calibration guides
├── QUICK_SETUP.md            # Fast deployment instructions
└── CALIBRATION_GUIDE.md      # Sensor calibration procedures
```

## Contributing

This project is designed for educational and research purposes. Feel free to fork, modify, and adapt for your specific monitoring needs.

## License

Open source - suitable for academic and research applications.

---

**Built with ESP32 for reliable water monitoring and IoT data collection.**
