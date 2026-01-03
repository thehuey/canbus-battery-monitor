# Quick Start Guide

Get up and running with the eBike Battery CANBUS Monitor in 5 minutes.

## Hardware You Need

**Minimum:**

- ESP32 development board
- USB cable

**For Full Testing:**

- TJA1050 CAN transceiver module
- Breadboard and jumper wires
- 120Ω resistor

---

## Step 1: Install PlatformIO

### Option A: VS Code Extension (Easiest)

1. Install [Visual Studio Code](https://code.visualstudio.com/)
2. Open VS Code
3. Go to Extensions (`Ctrl+Shift+X`)
4. Search for **"PlatformIO IDE"**
5. Click **Install**
6. Wait for installation (3-5 minutes)

### Option B: Command Line

```bash
pip3 install -U platformio
```

---

## Step 2: Open Project

### VS Code

1. **File → Open Folder**
2. Navigate to `/workspace`
3. Click **Select Folder**
4. PlatformIO will auto-detect the project

### Command Line

```bash
cd /workspace
```

---

## Step 3: Build Firmware

### VS Code

1. Click the **✓** (checkmark) in the bottom status bar
2. Wait for build to complete (~2 minutes first time)
3. Look for **"SUCCESS"** message

### Command Line

```bash
pio run
```

**First build** downloads libraries and toolchain (5-10 minutes). Subsequent builds are faster (~30 seconds).

---

## Step 4: Connect ESP32

1. Connect ESP32 to computer via USB
2. Check that power LED lights up
3. Wait for drivers to install (Windows)

### Find Serial Port

**Linux:** `/dev/ttyUSB0` or `/dev/ttyACM0`
**macOS:** `/dev/cu.usbserial-*`
**Windows:** `COM3` or `COM4`

---

## Step 5: Upload Firmware

### VS Code

1. Click the **→** (arrow) in the bottom status bar
2. Wait for upload (~30 seconds)
3. Look for **"SUCCESS"** message

### Command Line

```bash
pio run --target upload
```

**Troubleshooting Upload:**

- Press and hold **BOOT** button on ESP32
- Click **Upload**
- Wait for "Connecting..."
- Release **BOOT** button

---

## Step 6: Monitor Serial Output

### VS Code

1. Click the **🔌** (plug) icon in the bottom status bar
2. Serial monitor opens automatically

### Command Line

```bash
pio device monitor --baud 115200
```

Press `Ctrl+C` to exit monitor.

---

## Expected Output

You should see:

```
=================================
eBike Battery CANBUS Monitor
=================================

Loading settings...
SettingsManager: Initializing...
Using default settings

[Network]
  WiFi SSID:
  MQTT Broker: :1883

[CAN Bus]
  Bitrate: 500000 bps

[Batteries]
  Active Count: 1
  Battery 0: Battery 1

Initializing 1 battery module(s)...
BatteryModule 0 (Battery 1): Initialized
CAN bus initialized successfully
System initialized successfully!

========== Battery Summary ==========
Battery 0 (Battery 1):
  Voltage: 0.00 V
  Current: 0.00 A
  Power: 0.00 W
  SOC: 0%
  ...
```

**This is normal!** Without sensors or CAN data, values will be zero.

---

## Next Steps: Test Individual Modules

### Test Settings Manager

```bash
# Build and upload
pio run -e test_settings --target upload

# Monitor output
pio device monitor
```

You should see:

- ✓ Settings loaded
- ✓ Settings modified
- ✓ Settings saved
- ✓ Settings reloaded

### Test CAN Driver

```bash
# Build and upload
pio run -e test_can --target upload

# Monitor output
pio device monitor
```

You should see:

- ✓ CAN driver initialized
- Test messages sent every 2 seconds
- Statistics every 10 seconds

**Interactive commands:**

- Press **`e`** to export CAN log
- Press **`c`** to clear log
- Press **`r`** to reset statistics

### Test Battery Manager

```bash
# Build and upload
pio run -e test_battery --target upload

# Monitor output
pio device monitor
```

You should see:

- ✓ 2 batteries initialized
- Simulated discharge cycle
- Battery status every 5 seconds
- Warnings at low voltage/SOC

---

## Hardware Connection (Optional)

To test with real CAN hardware:

### Basic CAN Transceiver Connection

```
ESP32          TJA1050       Power
Pin 5 (TX) ──→ TXD
Pin 4 (RX) ←── RXD
GND        ──→ GND
               VCC       ←── 5V
```

### CAN Bus Termination

Add 120Ω resistor between CANH and CANL if this is the end of the bus.

### Verify Connection

With CAN hardware connected, you should see:

- No bus-off errors
- Messages on CAN bus (if other devices present)
- CAN logger capturing traffic

---

## Troubleshooting

### Build Fails

**"Library not found"**

```bash
# Clean and rebuild
rm -rf .pio
pio run
```

**"Platform not installed"**

```bash
pio pkg install --platform espressif32
```

### Upload Fails

**"Serial port not found"**

- Check USB cable supports data (not just charging)
- Install USB-to-Serial drivers (CP2102, CH340, or FTDI)
- Try different USB port
- Open WSL terminal in Windows, execute
  ```bash
  modprobe cp210x
  chmod 666 /dev/ttyUSB0
  ```

**"Failed to connect"**

- Hold BOOT button during upload
- Try lower speed: Add to platformio.ini:
  ```ini
  upload_speed = 115200
  ```

**Linux permission denied**

```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### Runtime Issues

**"Guru Meditation Error"**

- Normal during development
- Check serial output for details
- Press EN/RST button to restart

**"SPIFFS mount failed"**

- First boot only, creates filesystem
- If persists: `pio run --target erase`

**"CAN bus-off"**

- Normal without CAN hardware
- With hardware: check termination, bitrate, wiring

---

## Quick Command Reference

```bash
# Build main firmware
pio run

# Upload main firmware
pio run --target upload

# Build and upload in one command
pio run --target upload

# Monitor serial
pio device monitor

# Build + upload + monitor
pio run --target upload && pio device monitor

# Build test example
pio run -e test_settings --target upload

# Clean build
rm -rf .pio && pio run

# Check serial port
pio device list

# Erase flash completely
pio run --target erase
```

---

## What's Working Right Now

✅ **Settings Manager**

- Load/save configuration to NVS flash
- Factory defaults
- Multi-battery settings

✅ **CAN Driver**

- 500 kbps TWAI interface
- Message send/receive
- CSV logging to SPIFFS
- Bus-off recovery

✅ **Battery Manager**

- 1-5 battery support
- Voltage, current, power tracking
- Health monitoring
- Aggregate calculations

---

## What's Not Yet Implemented

⚠️ **Sensors**

- ACS712 current sensing
- Voltage measurement
- ADC calibration

⚠️ **Network**

- WiFi connectivity
- MQTT publishing
- Web dashboard
- WebSocket updates

⚠️ **Web Interface**

- HTML dashboard
- Real-time graphs
- Configuration UI

---

## Getting Help

1. **Check Serial Monitor** - Most issues show error messages
2. **Read BUILD_AND_TEST.md** - Detailed testing guide
3. **Check PROGRESS.md** - Implementation status
4. **Review module README.md** - API documentation
5. **Check CLAUDE.md** - Hardware specifications

---

## Success!

If you see the startup banner and battery summary without errors, you're ready to:

1. **Connect CAN hardware** for real battery monitoring
2. **Implement sensors** to measure current/voltage
3. **Add networking** for WiFi and MQTT
4. **Build web UI** for remote monitoring

See **BUILD_AND_TEST.md** for detailed testing procedures.

Happy monitoring! 🔋⚡
