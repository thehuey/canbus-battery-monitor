# Build and Test Guide

This guide explains how to build, upload, and test the eBike Battery CANBUS Monitor firmware.

## Prerequisites

### Hardware Required

**Minimum for testing:**
- ESP32 development board (any variant with TWAI/CAN)
- USB cable for programming
- Computer with USB port

**For full CAN testing:**
- TJA1050 or MCP2551 CAN transceiver module
- Breadboard and jumper wires
- 120Ω resistor (for CAN bus termination)
- Optional: Another CAN device or CAN USB adapter for testing

**For sensor testing:**
- ACS712 current sensor modules
- Voltage divider resistors (for battery voltage sensing)
- Power supply (5V for sensors)

### Software Required

1. **PlatformIO** (one of these options):
   - PlatformIO IDE (VS Code extension) - **RECOMMENDED**
   - PlatformIO Core (command line)
   - PlatformIO IDE for Atom

2. **USB-to-Serial drivers** (if needed):
   - CP2102 driver for NodeMCU-style boards
   - CH340 driver for clone boards
   - FTDI driver for FTDI-based boards

---

## Installation

### Option 1: PlatformIO IDE (VS Code Extension) - RECOMMENDED

1. **Install Visual Studio Code**
   - Download from https://code.visualstudio.com/
   - Install for your platform

2. **Install PlatformIO Extension**
   - Open VS Code
   - Click Extensions icon (or `Ctrl+Shift+X`)
   - Search for "PlatformIO IDE"
   - Click "Install"
   - Wait for installation to complete (may take several minutes)

3. **Verify Installation**
   - Click the PlatformIO icon in the left sidebar (alien head)
   - You should see the PlatformIO home screen

### Option 2: PlatformIO Core (Command Line)

```bash
# Install Python 3.6+ if not already installed
python3 --version

# Install PlatformIO Core
pip3 install -U platformio

# Verify installation
pio --version
```

---

## Opening the Project

### VS Code Method

1. **Open Project Folder**
   - File → Open Folder
   - Navigate to `/workspace` (the project root)
   - Click "Select Folder"

2. **PlatformIO should auto-detect**
   - Look for "PlatformIO: Project Tasks" in the bottom status bar
   - If not visible, reload the window (Ctrl+Shift+P → "Reload Window")

### Command Line Method

```bash
cd /workspace
pio project init --ide vscode
```

---

## Building the Project

### VS Code Method

1. **Open PlatformIO**
   - Click the PlatformIO icon (left sidebar)
   - Or click the Home icon in the bottom status bar

2. **Build the Firmware**
   - Navigate to: PROJECT TASKS → esp32dev → General → Build
   - Click "Build"
   - OR use the checkmark icon in the bottom status bar

3. **Watch the Output**
   - Build output appears in the terminal
   - First build downloads dependencies (may take 5-10 minutes)
   - Success message: "SUCCESS"

### Command Line Method

```bash
# Navigate to project directory
cd /workspace

# Build the project
pio run

# You should see output like:
# Processing esp32dev (platform: espressif32; board: esp32dev; framework: arduino)
# ...
# Building .pio/build/esp32dev/firmware.bin
# SUCCESS
```

### Expected Build Output

```
Dependency Graph
|-- ArduinoJson @ 7.x.x
|-- AsyncTCP @ ...
|-- ESP Async WebServer @ ...
|-- PubSubClient @ 2.8
|-- arduino-CAN @ 0.3.1

Building in release mode
...
Linking .pio/build/esp32dev/firmware.elf
Building .pio/build/esp32dev/firmware.bin
=========================== [SUCCESS] Took X.XX seconds ===========================
```

---

## Uploading to ESP32

### Prepare Hardware

1. **Connect ESP32 to Computer**
   - Use a USB cable with data lines (not charge-only)
   - Connect to computer USB port
   - ESP32 power LED should light up

2. **Identify Serial Port**
   - **Linux**: Usually `/dev/ttyUSB0` or `/dev/ttyACM0`
   - **macOS**: Usually `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`
   - **Windows**: Usually `COM3`, `COM4`, etc.

3. **Check Port in Terminal** (optional)
   ```bash
   # Linux/macOS
   ls /dev/tty*

   # Windows (PowerShell)
   [System.IO.Ports.SerialPort]::getportnames()
   ```

### Upload Firmware

#### VS Code Method

1. **Open PlatformIO Tasks**
   - Click PlatformIO icon
   - Navigate to: PROJECT TASKS → esp32dev → General → Upload

2. **Put ESP32 in Upload Mode** (if auto-reset doesn't work)
   - Hold BOOT button
   - Press and release EN/RST button
   - Release BOOT button
   - Board should now be in upload mode

3. **Click Upload**
   - Firmware will compile (if needed) and upload
   - You should see progress bar
   - Success message: "SUCCESS"

#### Command Line Method

```bash
# Upload firmware
pio run --target upload

# Specify port manually if auto-detection fails
pio run --target upload --upload-port /dev/ttyUSB0

# Monitor after upload
pio device monitor
```

### Upload Troubleshooting

**"Serial port not found"**
- Check USB cable (must support data)
- Install USB-to-Serial drivers
- Check Device Manager (Windows) or `dmesg` (Linux)
- Try different USB port

**"Failed to connect to ESP32"**
- Press and hold BOOT button during upload
- Check that no serial monitor is open
- Verify correct board selected in platformio.ini
- Try lower upload speed: add `upload_speed = 115200` to platformio.ini

**"Permission denied" (Linux)**
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER
# Log out and back in
```

---

## Testing Phase 1: Basic Functionality (No Hardware)

### Upload and Monitor

```bash
# Upload firmware
pio run --target upload

# Open serial monitor
pio device monitor --baud 115200
```

OR in VS Code: PROJECT TASKS → esp32dev → General → Monitor

### Expected Output

You should see:

```
=================================
eBike Battery CANBUS Monitor
=================================

Loading settings...
SettingsManager: Initializing...
SettingsManager: No valid settings found, using defaults
Warning: Using default settings

========== Current Settings ==========

[Network]
  WiFi SSID:
  WiFi Password: (empty)
  MQTT Broker: :1883
  MQTT Topic Prefix: ebike
  ...

[Batteries]
  Active Count: 1

  Battery 0:
    Enabled: Yes
    Name: Battery 1
    ...

======================================

Initializing 1 battery module(s)...
BatteryModule 0 (Battery 1): Initialized
BatteryManager: Initialized with 1 battery module(s)
GPIO pins configured
Initializing CAN bus...
CANLogger: Initializing...
CANLogger: SPIFFS - Total: XXXXX bytes, Used: XXX bytes
CANLogger: Initialized, logging to /canlog.csv
CANDriver: Initializing at 500000 bps...
CANDriver: Initialized successfully at 500000 bps
CAN bus initialized successfully
...
System initialized successfully!
=================================
```

### Verify Modules Work

**Settings Manager:**
- Check that settings print correctly
- Verify default values are loaded

**Battery Manager:**
- Battery count should match settings (default: 1)
- Battery names should show

**CAN Driver:**
- Should initialize even without hardware
- May show errors if no CAN transceiver connected (normal)

**Logger:**
- SPIFFS should mount successfully
- Log file created

---

## Testing Phase 2: Individual Modules

### Test 1: Settings Manager

**Upload Settings Test Example:**

```bash
# Modify platformio.ini temporarily
# Change: src/main.cpp
# To: examples/settings_test.cpp

# Edit platformio.ini and add:
# build_src_filter = +<../examples/settings_test.cpp>

# Or create a new environment:
[env:test_settings]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = ${env:esp32dev.lib_deps}
build_src_filter = +<../examples/settings_test.cpp>

# Build and upload
pio run -e test_settings --target upload
pio device monitor --baud 115200
```

**Expected Output:**
- Settings loaded/created
- Settings printed
- Settings modified
- Settings saved
- Settings reloaded successfully

**Verify:**
- Settings persist after reboot
- Factory reset works
- Invalid settings are rejected

### Test 2: CAN Driver (Without Hardware)

The main firmware will show CAN initialization. For full testing:

**Connect CAN Hardware (Optional):**

```
ESP32 Pin 5 (TX) ──→ TJA1050 TXD
ESP32 Pin 4 (RX) ←── TJA1050 RXD
ESP32 GND      ──→ TJA1050 GND
TJA1050 VCC    ←── 5V power supply
```

**Test CAN Example:**

Modify platformio.ini to build `examples/can_test.cpp`:

```bash
pio run -e test_can --target upload
pio device monitor
```

**Expected Output (without hardware):**
- CAN driver initializes
- No bus-off errors (if no hardware)
- Test messages sent every 2 seconds
- Statistics printed every 10 seconds

**Expected Output (with hardware):**
- CAN messages sent and received
- Logger captures messages
- Press 'e' in serial monitor to export log
- Press 'c' to clear log
- Press 'r' to reset statistics

### Test 3: Battery Module

**Upload Battery Test Example:**

```bash
# Build battery test example
pio run -e test_battery --target upload
pio device monitor
```

**Expected Output:**
- 2 batteries initialized
- Voltage/current updates work
- CAN data updates work
- Aggregate calculations correct
- Health monitoring functional
- Simulated discharge cycle runs

**Verify:**
- Total power = sum of individual batteries
- Average voltage calculated correctly
- Health status detects errors
- Data freshness checking works

---

## Testing Phase 3: Integrated System

### Setup Test Environment

**Minimal CAN Test Setup:**

```
┌──────────┐         ┌──────────┐         ┌──────────┐
│  ESP32   │         │ TJA1050  │         │ CAN Bus  │
│  Pin 5   ├────────→│   TXD    │         │          │
│  Pin 4   │←────────┤   RXD    │         │   H ○────┤ 120Ω to L
│  GND     ├────────→│   GND    │         │          │
└──────────┘         │   VCC    │←── 5V   │   L ○────┘
                     │  CANH    ├────────→│    H     │
                     │  CANL    ├────────→│    L     │
                     └──────────┘         └──────────┘
```

**With CAN USB Adapter (for testing):**
- Connect CAN H/L to USB adapter
- Use `candump` (Linux) or CAN analyzer software
- Send test messages to ESP32
- Verify ESP32 logs messages correctly

### Monitor System Operation

```bash
pio device monitor --baud 115200
```

**Watch For:**
1. **Startup sequence** completes successfully
2. **CAN statistics** every 30 seconds
3. **Battery summary** every 60 seconds
4. **Health checks** every 30 seconds
5. **No error messages** or warnings
6. **Free heap** stays above 20,000 bytes

### Send Test CAN Messages

**Using CAN USB adapter (Linux):**

```bash
# Install can-utils
sudo apt-get install can-utils

# Set up CAN interface
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up

# Send test battery status message
cansend can0 100#5204D07D55414302

# Breakdown:
# 100 = CAN ID (battery 0 status)
# 52 04 = 1106 = 110.6V (0x0452 = 1106, 1106 * 0.1 = 110.6V)
# D0 7D = 32208 = 20.8A (32208 - 32000 = 208, 208 * 0.1 = 20.8A)
# 55 = 85% SOC
# 41 = 65 = 25°C (65 - 40 = 25)
# 43 = 67 = 27°C (67 - 40 = 27)
# 02 = Status: DISCHARGING

# Monitor CAN bus
candump can0
```

**Expected ESP32 Response:**
- Message received and logged
- Battery data parsed correctly
- Battery 0 shows: 110.6V, 20.8A, 85%, 25°C, 27°C, DISCHARGING

---

## Common Issues and Solutions

### Build Errors

**"Library not found"**
- Delete `.pio` directory and rebuild
- Check internet connection (downloads libraries)
- Run: `pio pkg install`

**"Multiple definition errors"**
- Check for duplicate .cpp files
- Verify only one main() function
- Check build filters in platformio.ini

**"ESP32 core not found"**
```bash
pio pkg install --platform espressif32
```

### Runtime Issues

**"Guru Meditation Error"**
- Stack overflow - increase task stack sizes
- Memory corruption - check buffer overruns
- Watchdog timeout - reduce delay() usage

**"SPIFFS mount failed"**
- Upload filesystem: `pio run --target uploadfs`
- Partition table may need adjustment
- Check SPIFFS size in platformio.ini

**"CAN bus-off errors"**
- Check CAN termination (120Ω resistor)
- Verify correct bitrate (500kbps)
- Check CANH/CANL connections
- Ensure no shorts on CAN lines

**"Settings not saving"**
- NVS partition may be corrupted
- Erase flash: `pio run --target erase`
- Re-upload firmware

**"Low heap warnings"**
- Normal on ESP32, just a warning
- Reduce CAN log buffer size if needed
- Monitor over time to ensure not decreasing

---

## Serial Monitor Commands

### Interactive Testing

While monitoring serial output, you can interact with the test examples:

**CAN Test Example:**
- Press `e` - Export CAN log to serial
- Press `c` - Clear CAN log
- Press `r` - Reset statistics

**Settings Test Example:**
- Runs automatically through test sequence
- Check serial output for results

**Battery Test Example:**
- Automatic simulation runs
- Watch discharge cycle progress
- Observe health warnings at low voltage/SOC

---

## Debugging Tips

### Enable Debug Output

Edit `platformio.ini`:

```ini
build_flags =
    -DCORE_DEBUG_LEVEL=5  ; Change from 3 to 5 for verbose
    -DCONFIG_ARDUHAL_LOG_COLORS=1
```

### Check Memory Usage

```bash
# After build, check sizes
pio run --target size

# Monitor heap in real-time
# Already implemented in main loop - watch serial output
```

### CAN Message Debugging

Add this to `setupCANBus()` in main.cpp:

```cpp
canDriver.setMessageCallback([](const CANMessage& msg) {
    // Print every received message
    Serial.printf("RX: ID=0x%03X, DLC=%d, Data=", msg.id, msg.dlc);
    for (uint8_t i = 0; i < msg.dlc; i++) {
        Serial.printf("%02X ", msg.data[i]);
    }
    Serial.println();

    canLogger.logMessage(msg);
});
```

---

## Advanced Testing

### Load Testing

**Simulate High CAN Traffic:**

```bash
# Using can-utils on Linux
while true; do
    cansend can0 100#0102030405060708
    cansend can0 101#0102030405060708
    sleep 0.01  # 100 messages/second
done
```

Monitor ESP32:
- Check RX dropped count (should be 0)
- Verify logging keeps up
- Monitor heap stability

### Stress Testing

**Long-term Stability Test:**
- Run for 24+ hours
- Monitor heap (should not decrease)
- Check for CAN errors
- Verify SPIFFS log rotation works
- Check for memory leaks

---

## Next Steps

Once basic testing is complete:

1. **Implement Sensors** - Add ACS712 and voltage sensing
2. **Add Network** - WiFi, MQTT, web server
3. **Create Web UI** - Dashboard interface
4. **Calibrate** - Sensor calibration procedures
5. **Real Battery Testing** - Connect to actual eBike battery

---

## Getting Help

If you encounter issues:

1. **Check serial output** for error messages
2. **Review CLAUDE.md** for hardware specifications
3. **Check PROGRESS.md** for implementation status
4. **Read module README.md** files for API details
5. **Review example code** for usage patterns

Common serial output to report when asking for help:
- Full startup sequence
- Error messages
- Free heap value
- CAN statistics
- Build output (if build fails)
