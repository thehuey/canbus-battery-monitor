# eBike Battery CANBUS Monitor

## Project Overview

This project creates a hardware and firmware solution for monitoring eBike battery modules via CANBUS. The system captures battery telemetry, measures voltage and current, logs CANBUS messages, and provides a web interface for real-time monitoring. Data is also published to an MQTT broker for integration with home automation or data logging systems.

### Key Specifications
- **CANBUS Bitrate**: 500 kbps
- **Battery Modules**: 1 to 5 (modular, runtime configurable)
- **Protocol Status**: Partially reverse-engineered, ongoing discovery

## Hardware Components

### Microcontroller Options
- **ESP32** (preferred): Dual-core, built-in WiFi, sufficient GPIO, hardware SPI for CAN controller
- **ESP8266**: Single-core, built-in WiFi, limited GPIO (backup option, may require I2C expander)

### CAN Interface
- **TJA1050**: CAN transceiver (3.3V logic compatible with level shifting, or use with 5V supply and voltage divider on RX)
- **MCP2515**: SPI-based CAN controller (required since ESP32/ESP8266 lack native CAN peripheral that works with TJA1050 directly)
  - Note: ESP32 has a built-in TWAI (CAN) controller that can work with appropriate transceivers

### Current Sensing
- **ACS712**: Hall-effect current sensor modules (one per battery module, up to 5)
  - ACS712-05A: ±5A range, 185mV/A sensitivity
  - ACS712-20A: ±20A range, 100mV/A sensitivity
  - ACS712-30A: ±30A range, 66mV/A sensitivity
  - Select based on expected battery current draw
- **Modular Design**: System auto-detects connected sensors at startup

### Power Supply Considerations
- ESP32/ESP8266 operate at 3.3V logic
- TJA1050 requires 5V supply
- ACS712 requires 5V supply, outputs 0-5V (needs voltage divider for ESP32 ADC)
- Consider isolated power if connecting to high-voltage battery systems

## Architecture

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Battery Pack   │────▶│   TJA1050       │────▶│     ESP32       │
│  (CANBUS)       │     │   Transceiver   │     │                 │
└─────────────────┘     └─────────────────┘     │  ┌───────────┐  │
                                                │  │ TWAI/CAN  │  │
┌─────────────────┐                             │  │ Controller│  │
│  ACS712         │────────────────────────────▶│  └───────────┘  │
│  Current Sensor │     (Analog via divider)    │                 │
└─────────────────┘                             │  ┌───────────┐  │
                                                │  │   WiFi    │──┼──▶ MQTT Broker
                                                │  │  + HTTP   │──┼──▶ Web Browser
                                                │  └───────────┘  │
                                                └─────────────────┘
```

## Pin Assignments (ESP32 - Suggested)

| Function              | GPIO  | Notes                              |
|-----------------------|-------|------------------------------------|
| CAN TX                | 5     | To TJA1050 TXD                     |
| CAN RX                | 4     | From TJA1050 RXD (via divider)     |
| ACS712 Battery 1      | 34    | ADC1_CH6, input only               |
| ACS712 Battery 2      | 35    | ADC1_CH7, input only               |
| ACS712 Battery 3      | 32    | ADC1_CH4                           |
| ACS712 Battery 4      | 33    | ADC1_CH5                           |
| ACS712 Battery 5      | 36    | ADC1_CH0, input only (VP)          |
| Voltage Sense Batt 1  | 39    | ADC1_CH3, input only (VN)          |
| Voltage Sense Common  | 25    | ADC2_CH8 (or use multiplexer)      |
| Status LED            | 2     | Built-in LED on most boards        |

**Note**: For 5 batteries with individual voltage sensing, consider using an analog multiplexer (CD74HC4067) to expand ADC channels, or share a single voltage sense line if batteries are in parallel.

## Software Architecture

### Core Modules

```
src/
├── main.cpp                 # Application entry point, task creation
├── config/
│   ├── config.h             # Compile-time defaults, pin definitions
│   ├── settings.h           # Settings struct definitions
│   └── settings.cpp         # NVS load/save, defaults
├── can/
│   ├── can_driver.cpp       # TWAI init at 500kbps, RX/TX tasks
│   ├── can_message.h        # CAN frame struct, ring buffer
│   ├── can_parser.cpp       # Protocol decoder (extensible)
│   ├── can_parser.h         # Parser interface, message handlers
│   └── can_logger.cpp       # SPIFFS logging, CSV export
├── battery/
│   ├── battery_manager.h    # Multi-battery orchestration
│   ├── battery_manager.cpp  # Manages 1-5 battery modules
│   ├── battery_module.h     # Single battery data structure
│   └── battery_module.cpp   # Per-battery readings, state
├── sensors/
│   ├── adc_manager.cpp      # ESP32 ADC configuration, sampling
│   ├── current_sensor.h     # ACS712 interface
│   ├── current_sensor.cpp   # Reading, calibration, averaging
│   ├── voltage_sensor.h     # Voltage divider interface
│   └── voltage_sensor.cpp   # Voltage reading and scaling
├── network/
│   ├── wifi_manager.cpp     # STA + AP mode, auto-reconnect
│   ├── mqtt_client.cpp      # Publishing, topic formatting
│   ├── web_server.cpp       # Async HTTP handlers
│   ├── websocket.cpp        # Real-time push to clients
│   └── api_handlers.cpp     # REST API implementations
├── web/                     # Static files (SPIFFS)
│   ├── index.html           # Dashboard SPA
│   ├── app.js               # Frontend logic
│   └── style.css            # Minimal styling
└── utils/
    ├── ring_buffer.h        # Template ring buffer
    ├── moving_average.h     # Configurable sample window
    └── task_utils.h         # FreeRTOS helpers
```

### Modular Battery Design

The system dynamically handles 1-5 batteries:

```cpp
// battery_manager.h
class BatteryManager {
public:
    void begin(uint8_t num_batteries);
    void update();  // Called from main loop/task
    
    BatteryModule* getBattery(uint8_t index);
    uint8_t getActiveBatteryCount();
    float getTotalPower();
    
    // Configuration
    void enableBattery(uint8_t index, bool enabled);
    void setBatteryName(uint8_t index, const char* name);
    void calibrateCurrent(uint8_t index);  // Zero-point cal
    
private:
    BatteryModule batteries[MAX_BATTERY_MODULES];
    uint8_t active_count;
};
```

### Data Flow

1. **CAN Reception**: TWAI ISR → frame queue → parser task → decoded data + raw log
2. **Sensor Sampling**: 10ms timer → ADC reads → moving average → battery module update
3. **Battery Manager**: Aggregates sensor + CAN data per battery, calculates power
4. **MQTT Publishing**: 1s timer → JSON build per battery → publish to broker
5. **WebSocket Push**: 500ms timer → JSON status → all connected clients
6. **CAN Logging**: Separate task → write to SPIFFS ring file → rotation on 80% full

## Configuration System

### WiFi Configuration
- On first boot or config reset, device creates AP: `eBikeMonitor-XXXX`
- Connect to AP, navigate to `192.168.4.1` for configuration portal
- Enter home WiFi credentials, MQTT broker details
- Settings stored in NVS (non-volatile storage)

### Runtime Settings (stored in NVS)
```cpp
#define MAX_BATTERY_MODULES 5

struct BatteryConfig {
    bool enabled;
    char name[16];                  // e.g., "Battery 1", "Front", "Rear"
    float current_cal_offset;       // Zero-current offset (mV)
    float current_cal_scale;        // mV per Amp
    float voltage_cal_scale;        // Voltage divider ratio
    uint32_t can_base_id;           // Base CAN ID for this battery (0 = auto)
};

struct Settings {
    // Network
    char wifi_ssid[32];
    char wifi_password[64];
    char mqtt_broker[64];
    uint16_t mqtt_port;
    char mqtt_topic_prefix[32];
    
    // CAN Configuration
    uint32_t can_bitrate;           // Fixed at 500000
    
    // Timing
    uint16_t publish_interval_ms;   // MQTT publish rate (default: 1000)
    uint16_t sample_interval_ms;    // ADC sample rate (default: 100)
    uint16_t web_refresh_ms;        // WebSocket push rate (default: 500)
    
    // Battery Modules (modular: 1-5)
    uint8_t num_batteries;          // Active battery count
    BatteryConfig batteries[MAX_BATTERY_MODULES];
};
```

## Web Interface

### Endpoints

| Endpoint                  | Method | Description                            |
|---------------------------|--------|----------------------------------------|
| `/`                       | GET    | Main dashboard (static HTML)           |
| `/api/status`             | GET    | All readings, WiFi, uptime             |
| `/api/battery/:id`        | GET    | Single battery status                  |
| `/api/batteries`          | GET    | All battery statuses                   |
| `/api/canlog`             | GET    | Recent CAN messages (JSON array)       |
| `/api/canlog?filter=0x100`| GET    | Filtered CAN messages by ID            |
| `/api/canlog/download`    | GET    | Download full log as CSV               |
| `/api/canlog/clear`       | POST   | Clear the CAN log buffer               |
| `/api/config`             | GET    | Current configuration                  |
| `/api/config`             | POST   | Update configuration                   |
| `/api/config/battery/:id` | POST   | Update single battery config           |
| `/api/calibrate/:id`      | POST   | Trigger zero-current calibration       |
| `/api/reset`              | POST   | Reboot device                          |
| `/ws`                     | WS     | WebSocket for real-time updates        |

### Dashboard Features
- Real-time voltage and current display per battery (WebSocket push)
- Combined power totals across all batteries
- CAN message live view with ID filtering and search
- Simple sparkline graphs for recent readings (last 5 minutes)
- Per-battery configuration and naming
- Calibration interface for current sensors
- Log download button (CSV format with timestamps)

## MQTT Topics

Topic structure supports multiple batteries with index or name:

```
ebike/battery/1/status            # Battery 1 status
ebike/battery/2/status            # Battery 2 status
ebike/battery/all/status          # Combined status (all batteries)
ebike/can/raw                     # Raw CAN frames (hex encoded)
ebike/can/parsed                  # Interpreted battery data
ebike/system/status               # Device health: uptime, heap, RSSI
ebike/system/config               # Current configuration (retained)
```

### Example Payloads

```json
// ebike/battery/1/status
{
    "id": 1,
    "name": "Front",
    "voltage": 52.4,
    "current": 3.2,
    "power": 167.68,
    "enabled": true,
    "timestamp": 1703789400
}

// ebike/battery/all/status
{
    "batteries": [
        {"id": 1, "name": "Front", "voltage": 52.4, "current": 3.2, "power": 167.68},
        {"id": 2, "name": "Rear", "voltage": 51.8, "current": 2.1, "power": 108.78}
    ],
    "total_power": 276.46,
    "timestamp": 1703789400
}

// ebike/can/raw
{
    "id": "0x123",
    "dlc": 8,
    "data": "0102030405060708",
    "timestamp": 1703789400123
}
```

## Development Guidelines

### Code Style
- Use Arduino framework with PlatformIO
- C++17 features allowed
- Prefer `const` and `constexpr` where possible
- Use meaningful names, avoid abbreviations except well-known ones (CAN, MQTT, etc.)
- Each module should have clear separation of concerns

### Error Handling
- WiFi disconnection: Automatic reconnection with exponential backoff
- MQTT disconnection: Queue messages locally, reconnect and publish
- CAN bus errors: Log error frames, continue operation
- Sensor faults: Report NaN/null, set error flag in status

### Memory Management
- Use static allocation where possible (ring buffers, etc.)
- Monitor free heap, warn if below threshold
- CAN log rotation when SPIFFS usage exceeds 80%

### Testing Approach
- Unit tests for parsers and utilities (PlatformIO native)
- Integration tests with CAN simulator
- Web interface tested in browser with mock API

## Build and Flash

```bash
# Install PlatformIO CLI or use VS Code extension

# In dev container, PlatformIO is located at:
# /home/node/.platformio/penv/bin/pio
# Or use the 'pio' command if it's in PATH

# Build
pio run

# Upload
pio run --target upload

# Monitor serial
pio device monitor --baud 115200

# Upload filesystem (web files)
pio run --target uploadfs
```

### WSL2 USB Device Access

If you're running in WSL2 (Windows Subsystem for Linux), USB devices are not automatically accessible. You have two options:

**Option 1: Use usbipd-win to share USB devices with WSL2**
```bash
# On Windows (PowerShell as Administrator):
# Install usbipd-win from https://github.com/dorssel/usbipd-win
winget install --interactive --exact dorssel.usbipd-win

# List USB devices
usbipd list

# Bind your ESP32 device (replace BUSID with your device's bus ID)
usbipd bind --busid <BUSID>

# Attach to WSL
usbipd attach --wsl --busid <BUSID>

# In WSL, verify the device appears
ls /dev/ttyUSB* || ls /dev/ttyACM*

# Update platformio.ini to use Linux port naming (e.g., /dev/ttyUSB0)
```

**Option 2: Use PlatformIO from Windows directly**
```powershell
# Install PlatformIO on Windows
pip install platformio

# Build and upload from Windows terminal
pio run --target upload
```

## Hardware Assembly Notes

### Voltage Divider for TJA1050 RX
The TJA1050 outputs 5V logic, ESP32 expects 3.3V max:
```
TJA1050 RXD ──┬── 10kΩ ──┬── ESP32 GPIO
              │          │
             GND       3.3V (via 20kΩ to GND for voltage divider)
```
Or use a proper level shifter for reliability.

### ACS712 Voltage Divider
ACS712 outputs 2.5V at 0A, swings ±1.5V. For ESP32 ADC (0-3.3V max):
```
ACS712 OUT ── 10kΩ ──┬── ESP32 ADC
                     │
                    20kΩ
                     │
                    GND
```
This scales 5V max to ~3.3V max.

### CANBUS Connection
- CANH and CANL from battery to TJA1050
- Ensure proper termination (120Ω between CANH and CANL if end of bus)
- Keep wires short and twisted for noise immunity

## Safety Considerations

⚠️ **Important Safety Notes:**

1. eBike batteries are high voltage (36V-72V typically) and high current
2. Never work on live battery connections
3. Use appropriate fusing on all connections
4. Isolate the monitoring circuit if possible
5. Ensure proper strain relief on all cables
6. Use appropriate enclosure rated for the environment
7. The ACS712 provides galvanic isolation for current sensing

## Future Enhancements (Out of Scope for v1)

- [ ] SD card logging for extended storage
- [ ] OTA firmware updates
- [ ] HTTPS/TLS for web interface
- [ ] MQTT authentication
- [ ] Battery state-of-charge estimation
- [ ] Multi-battery cell balancing data
- [ ] Mobile app companion
- [ ] Historical data graphing
- [ ] Alert notifications (low voltage, overcurrent)

## Dependencies

```ini
; platformio.ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    ESP Async WebServer
    AsyncTCP
    ArduinoJson
    PubSubClient
    ESP32-TWAI-CAN  ; or use built-in driver
monitor_speed = 115200
board_build.filesystem = spiffs
```

## References

- ESP32 TWAI documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/twai.html
- TJA1050 datasheet
- ACS712 datasheet and application notes
- MQTT specification: https://mqtt.org/

---

## CAN Protocol Reference (Reverse Engineered)

This section documents the known CAN message IDs and their data formats. Update this as new messages are decoded.

### Message Format Convention
- All values are **little-endian** unless noted otherwise
- Temperatures are in °C with offset (value - 40 = actual temp)
- Voltages are typically in 0.01V or 0.1V units
- Currents are typically in 0.1A units, signed

### Known Message IDs

#### 0x100 - Battery Status (Example - Update with actual)
| Byte | Description           | Units/Formula          |
|------|-----------------------|------------------------|
| 0-1  | Pack Voltage          | value × 0.1 = Volts    |
| 2-3  | Pack Current          | (value - 32000) × 0.1 = Amps (signed) |
| 4    | SOC                   | 0-100%                 |
| 5    | Temperature 1         | value - 40 = °C        |
| 6    | Temperature 2         | value - 40 = °C        |
| 7    | Status Flags          | See flags table        |

#### 0x101 - Cell Voltages Group 1 (Example)
| Byte | Description           | Units/Formula          |
|------|-----------------------|------------------------|
| 0-1  | Cell 1 Voltage        | value × 0.001 = Volts  |
| 2-3  | Cell 2 Voltage        | value × 0.001 = Volts  |
| 4-5  | Cell 3 Voltage        | value × 0.001 = Volts  |
| 6-7  | Cell 4 Voltage        | value × 0.001 = Volts  |

### Unknown/Undecoded Messages

Track observed but undecoded message IDs here for future analysis:

| ID     | DLC | Sample Data              | Notes / Observations    |
|--------|-----|--------------------------|-------------------------|
| 0x???  | 8   | XX XX XX XX XX XX XX XX  | Seen during charging    |
| 0x???  | 8   | XX XX XX XX XX XX XX XX  | Seen during discharge   |

### Status Flag Definitions (if applicable)

| Bit | Name              | Description                    |
|-----|-------------------|--------------------------------|
| 0   | CHARGING          | Battery is charging            |
| 1   | DISCHARGING       | Battery is discharging         |
| 2   | BALANCING         | Cell balancing active          |
| 3   | TEMP_WARNING      | Temperature outside normal     |
| 4   | OVER_VOLTAGE      | Pack/cell overvoltage          |
| 5   | UNDER_VOLTAGE     | Pack/cell undervoltage         |
| 6   | OVER_CURRENT      | Current limit exceeded         |
| 7   | ERROR             | General fault condition        |

### Reverse Engineering Notes

**Methodology:**
1. Log all CAN traffic during different states (idle, charging, discharging)
2. Use the web interface filtering to isolate message IDs
3. Compare byte patterns across different conditions
4. Look for correlations with known values (displayed SOC, etc.)

**Tools:**
- The web interface CAN log with filtering
- Export to CSV for analysis in spreadsheet
- Compare against any available battery documentation

**Tips:**
- Message IDs are often grouped (0x100-0x10F for one function)
- Some batteries use the ID to indicate which module is transmitting
- Watch for periodic vs event-driven messages
- Temperature sensors often have predictable patterns (room temp ~25°C = 65 raw)

---

## Glossary

- **CANBUS**: Controller Area Network, a vehicle bus standard
- **TWAI**: Two-Wire Automotive Interface, ESP32's CAN controller
- **DLC**: Data Length Code, number of bytes in CAN frame
- **NVS**: Non-Volatile Storage, ESP32's key-value flash storage
- **SPIFFS**: SPI Flash File System
- **MQTT**: Message Queuing Telemetry Transport, lightweight pub/sub protocol

---

## Architecture Decisions & Lessons Learned

### Browser-Side Protocol Parsing (not firmware)

Protocol parsing for CAN messages runs entirely in the browser, NOT on the ESP32. This was a deliberate architectural decision after discovering that adding protocol parsing endpoints to the firmware caused a **DRAM segment overflow (~21KB over)**. The ESP32 has very limited RAM and the protocol structures are large.

**How it works:**
1. Protocol JSON files live in `data/protocols/` on SPIFFS
2. Firmware serves them via `GET /api/protocols` (list) and `GET /api/protocols/<filename>` (fetch)
3. `protocol_manager.js` in the browser loads, indexes, and parses CAN messages client-side
4. The browser maps parsed fields to battery data (SOC, voltage, current, etc.)

**Key files:**
- `data/web/protocol_manager.js` — Browser-side protocol loading, parsing, and field extraction
- `data/protocols/dpower_48v_13s.json` — D-power 48V 13S battery protocol definition
- `data/protocols/generic_bms.json` — Generic BMS protocol definition
- `src/can/protocol.h` — Protocol struct definitions (used firmware-side for built-in protocols)
- `src/can/builtin_protocols.h/.cpp` — Compiled-in protocol definitions (DPOWER_48V_13S, GENERIC_BMS)
- `src/can/protocol_loader.h` — SPIFFS protocol file loader with `listCustomProtocols()`

### Web Server Static File Serving

The web server uses a **single wildcard `serveStatic()` call** instead of individual per-file routes:

```cpp
server_.serveStatic("/protocols/", SPIFFS, "/protocols/");
server_.serveStatic("/", SPIFFS, "/web/");
```

This means:
- **Any file added to `data/web/`** is automatically served at the root path (e.g., `/app.js`, `/style.css`)
- **Any file added to `data/protocols/`** is accessible at `/protocols/<filename>`
- **No firmware code changes needed** when adding new web files or protocol definitions — just `pio run --target uploadfs`
- The `/protocols/` route is registered BEFORE the `/` catch-all so it takes priority

### CAN Message Batching

WebSocket CAN messages are batched to prevent client disconnects. The firmware buffers up to 25 CAN messages and flushes periodically, sending them as a JSON array rather than individual frames. This prevents WebSocket queue overflow on busy CAN buses.

### ESP32 Memory Constraints

- **DRAM is extremely limited** (~320KB total, ~37.9% used currently)
- Protocol struct `Definition` with 8 messages × 8 fields is very large in static allocation
- Always prefer browser-side processing over firmware for anything the browser can handle
- Use `constexpr` sizes and static allocation — avoid dynamic allocation where possible
- Monitor RAM usage: `pio run` shows RAM percentage in build output

### CAN Bus Monitor Features

The web UI CAN bus monitor (`app.js`, `dev_tools.js`) supports:
- **Include filter**: Show only specific CAN IDs (comma-separated)
- **Exclude filter**: Hide specific CAN IDs (comma-separated)
- **Extended CAN IDs**: Both standard (11-bit, 0x000-0x7FF) and extended (29-bit, 0x00000000-0x1FFFFFFF)
- **Batch message sender**: Send multiple CAN messages from a textarea, one per line
- **Hex data formats**: Both space-separated (`A2 00 00 00`) and continuous (`A2000000A2`)
- **localStorage persistence**: Dev tools history persists across browser sessions
- **Message counter**: Uses server-side authoritative count only (not local JS counter) to prevent overcounting

### Web UI File Structure

```
web_src/                        # JS source files (NOT deployed directly)
├── protocol_manager.js         # Browser-side protocol parsing
├── can_analyzer.js             # CAN analysis utilities
├── dev_tools.js                # CAN message sender, batch tools
└── app.js                      # Core frontend logic, WebSocket handling

scripts/
└── bundle_js.py                # PlatformIO pre-build: concatenates JS -> bundle.js

data/
├── web/                        # Served at / (root path)
│   ├── index.html              # Main dashboard SPA
│   ├── bundle.js               # Auto-generated by bundle_js.py (do not edit)
│   ├── style.css               # Styling
│   └── FEATURES.md             # Feature documentation
├── protocols/                  # Served at /protocols/
│   ├── dpower_48v_13s.json     # D-power battery protocol
│   ├── generic_bms.json        # Generic BMS protocol
│   └── SCHEMA.md               # Protocol JSON schema docs
```

### Build & Deploy

```bash
# PlatformIO binary location in dev container:
/home/node/.platformio/penv/bin/pio

# Build firmware
pio run

# Upload firmware only
pio run --target upload

# Upload web files and protocol definitions (SPIFFS filesystem)
pio run --target uploadfs

# Both (firmware changes + new web files)
pio run --target upload && pio run --target uploadfs
```

### Common Pitfalls

1. **New web files return 404**: No longer an issue — wildcard `serveStatic("/", SPIFFS, "/web/")` serves all files. But you still need to `uploadfs` to get them onto the device.
2. **Message counter jumping in chunks**: Fixed by removing local JS counter — only use server's authoritative count from `can_logger_`.
3. **DRAM overflow on new features**: Before adding firmware features, check build output for RAM%. If close to limit, consider browser-side implementation instead.
4. **Hex data parsing errors**: `parseDataBytes()` in `dev_tools.js` handles both space-separated and continuous hex formats.
5. **Protocol not loading in browser**: Check browser console for fetch errors. The `/api/protocols` endpoint must return valid JSON listing available files, and each file must be accessible at `/api/protocols/<filename>`.
