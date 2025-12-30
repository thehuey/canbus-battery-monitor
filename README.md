# eBike Battery CANBUS Monitor

ESP32-based monitoring system for eBike battery modules with CAN bus interface, WiFi connectivity, and MQTT integration.

## Project Status

**Phase: Initial Setup Complete**

The project structure has been created with all necessary directories and header files. The next phase is implementing the core functionality.

## Features

- Monitor 1-5 battery modules via CAN bus (500 kbps)
- ACS712 current sensing for each battery
- WiFi connectivity with web dashboard
- Real-time WebSocket updates
- MQTT publishing for home automation
- CAN message logging and protocol reverse engineering
- Configurable via web interface and NVS storage

## Hardware Requirements

- ESP32 development board
- TJA1050 CAN transceiver
- ACS712 current sensor modules (one per battery)
- Voltage dividers for safe ADC reading
- eBike battery pack(s) with CAN interface

See [CLAUDE.md](CLAUDE.md) for detailed hardware specifications and pin assignments.

## Directory Structure

```
.
├── platformio.ini          # PlatformIO configuration
├── CLAUDE.md               # Comprehensive project specification
├── README.md               # This file
├── src/
│   ├── main.cpp            # Application entry point
│   ├── config/             # Configuration and settings
│   │   ├── config.h
│   │   └── settings.h
│   ├── can/                # CAN bus driver and protocol
│   │   ├── can_message.h
│   │   └── can_parser.h
│   ├── battery/            # Battery management
│   │   ├── battery_module.h
│   │   └── battery_manager.h
│   ├── sensors/            # ADC and sensor interfaces
│   │   ├── current_sensor.h
│   │   └── voltage_sensor.h
│   ├── network/            # WiFi, MQTT, and web server (TBD)
│   └── utils/              # Utilities and helpers
│       ├── ring_buffer.h
│       └── moving_average.h
├── data/
│   └── web/                # Static web files for SPIFFS (TBD)
└── include/                # Additional headers
```

## Build Instructions

### Quick Start (5 minutes)

See **[QUICKSTART.md](QUICKSTART.md)** for step-by-step instructions to get running fast.

### Detailed Testing Guide

See **[BUILD_AND_TEST.md](BUILD_AND_TEST.md)** for comprehensive build, upload, and testing instructions.

### Prerequisites

1. Install [PlatformIO](https://platformio.org/install)
2. ESP32 development board
3. USB cable

### Basic Commands

```bash
# Build the project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200

# Build + upload + monitor
pio run --target upload && pio device monitor
```

### Test Individual Modules

```bash
# Test settings manager
pio run -e test_settings --target upload

# Test CAN driver
pio run -e test_can --target upload

# Test battery manager
pio run -e test_battery --target upload
```

## Development Roadmap

### Phase 1: Project Setup ✅
- [x] PlatformIO project structure
- [x] Directory layout
- [x] Core header files
- [x] Main application skeleton

### Phase 2: Core Implementation (Next)
- [ ] Settings manager with NVS
- [ ] CAN driver (TWAI)
- [ ] CAN parser
- [ ] Battery module implementation
- [ ] Sensor drivers

### Phase 3: Network Stack
- [ ] WiFi manager
- [ ] MQTT client
- [ ] Web server
- [ ] WebSocket implementation

### Phase 4: Web Interface
- [ ] Dashboard HTML/CSS/JS
- [ ] Real-time updates
- [ ] Configuration interface
- [ ] CAN log viewer

### Phase 5: Testing & Refinement
- [ ] Integration testing
- [ ] CAN protocol reverse engineering
- [ ] Calibration procedures
- [ ] Documentation

## Configuration

On first boot, the device creates a WiFi access point:
- SSID: `eBikeMonitor-XXXX`
- Password: `ebike123`
- IP: `192.168.4.1`

Connect to configure your home WiFi and MQTT broker settings.

## Safety Notes

⚠️ **WARNING**: eBike batteries are high voltage and high current devices. Always follow proper safety procedures. See [CLAUDE.md](CLAUDE.md) for detailed safety considerations.

## License

[Add your license here]

## Contributing

[Add contribution guidelines here]
