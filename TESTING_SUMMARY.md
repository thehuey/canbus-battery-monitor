# Testing Summary

This document provides a quick overview of what you can test right now.

## What's Ready to Test

### ✅ Main Firmware (Full Integration)

**Command:**
```bash
pio run --target upload && pio device monitor
```

**What works:**
- Settings load from NVS (or defaults on first boot)
- Battery manager initializes (1-5 batteries configurable)
- CAN driver starts at 500 kbps
- CAN logger writes to SPIFFS
- Status printed every 60 seconds
- Health monitoring every 30 seconds

**Expected serial output:**
```
=================================
eBike Battery CANBUS Monitor
=================================

Loading settings...
SettingsManager: Using default settings

[Network], [CAN Bus], [Batteries] sections print...

Initializing 1 battery module(s)...
CAN bus initialized successfully
System initialized successfully!

CAN Stats - RX: 0, TX: 0
Battery Summary every 60s
```

**Note:** Values will be zero without sensors or CAN data - this is normal!

---

### ✅ Test 1: Settings Manager

**Command:**
```bash
pio run -e test_settings --target upload && pio device monitor
```

**What it tests:**
- ✓ NVS initialization
- ✓ Default settings creation
- ✓ Settings modification
- ✓ Save to flash
- ✓ Reload verification
- ✓ Print formatted output

**Expected result:** All tests pass with ✓ checkmarks

**Runtime:** ~10 seconds

---

### ✅ Test 2: CAN Driver

**Command:**
```bash
pio run -e test_can --target upload && pio device monitor
```

**What it tests:**
- ✓ CAN driver init at 500 kbps
- ✓ CAN logger init
- ✓ Message transmission (every 2s)
- ✓ Message reception (if hardware connected)
- ✓ Logger buffering
- ✓ Statistics tracking

**Interactive:**
- Press `e` to export log as CSV
- Press `c` to clear log
- Press `r` to reset statistics

**With hardware:** Connect TJA1050 transceiver to see real messages

**Without hardware:** Sends test messages, shows stats

**Runtime:** Continuous

---

### ✅ Test 3: Battery Manager

**Command:**
```bash
pio run -e test_battery --target upload && pio device monitor
```

**What it tests:**
- ✓ Multi-battery initialization (2 batteries)
- ✓ Sensor updates
- ✓ CAN data updates
- ✓ Aggregate calculations
- ✓ Status flags
- ✓ Health monitoring
- ✓ Data freshness checking

**Features:**
- Simulates discharge cycle (100% → 0%)
- Battery status every 5 seconds
- Warnings at low voltage/SOC
- Auto-resets when discharged

**Runtime:** ~8 minutes per discharge cycle (continuous)

---

## Testing With Hardware

### Minimal CAN Setup

You need:
- ESP32 board
- TJA1050 CAN transceiver module ($2-5)
- Jumper wires
- 120Ω resistor (bus termination)

**Connections:**
```
ESP32 Pin 5 → TJA1050 TXD
ESP32 Pin 4 → TJA1050 RXD
ESP32 GND   → TJA1050 GND
5V          → TJA1050 VCC
```

**Bus termination:**
- Connect 120Ω resistor between CANH and CANL
- Only needed if you're at the end of the bus

### Testing CAN Communication

**Option 1: CAN USB Adapter (Linux)**
```bash
# Set up interface
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up

# Send test message
cansend can0 100#5204D07D55414302

# Monitor traffic
candump can0
```

**Option 2: Two ESP32 Boards**
- Flash one with main firmware
- Flash other with can_test example
- Connect both to same CAN bus
- They should communicate

**Option 3: Real eBike Battery**
- Connect to battery CAN bus (carefully!)
- Monitor messages with can_test
- Update parser as you reverse-engineer protocol

---

## Expected Test Results

### Settings Test Output

```
Test 1: Initializing SettingsManager...
✓ Settings loaded from NVS

Test 2: Print current settings
[Network], [CAN Bus], [Batteries] ...

Test 3: Modifying settings...
✓ Settings modified in memory

Test 4: Saving settings to NVS...
✓ Settings saved successfully

Test 5: Reloading settings to verify...
✓ Settings reloaded successfully

All tests completed!
```

### CAN Test Output

```
CAN Driver Test
========================================
Test 1: Initializing CAN logger...
✓ CAN logger initialized

Test 2: Initializing CAN driver at 500 kbps...
✓ CAN driver initialized

Test 3: Setting up message callback...
✓ Callback registered

Listening for CAN messages...
Sending test messages every 2 seconds...

TX: Sent test message #1
TX: Sent test message #2

--- CAN Statistics ---
RX Messages: 0
TX Messages: 2
Status: RUNNING

Press 'e' to export log
```

### Battery Test Output

```
Battery Module Test
========================================
Test 1: Initializing BatteryManager with 2 batteries...
✓ BatteryManager initialized

Test 2: Configuring battery names...
✓ Battery names configured

========== Battery Status ==========

Front Battery:
  Voltage:  52.00 V
  Current:  3.20 A
  Power:    166.40 W
  SOC:      98%
  Status:   DISCHARGING

Rear Battery:
  Voltage:  51.50 V
  Current:  2.56 A
  Power:    131.84 W
  SOC:      96%
  Status:   DISCHARGING

Aggregates:
  Total Power:    298.24 W
  Total Current:  5.76 A
  Health:         OK
```

---

## Debugging Failed Tests

### Settings Test Fails

**"Failed to open NVS"**
- Erase flash: `pio run --target erase`
- Re-upload firmware

**Settings don't persist**
- Check serial output for "saved successfully"
- NVS partition may be full - erase flash

### CAN Test Issues

**"CAN driver initialization failed"**
- Normal without hardware
- Check GPIO pins in config.h match your board

**"Bus-off errors"**
- Normal without hardware or termination
- With hardware: check wiring, termination, bitrate

**No messages received**
- Check transceiver connections
- Verify 500 kbps on both sides
- Add termination resistor

### Battery Test Issues

**Compilation errors**
- Check that can_message.h is included
- Verify all dependencies in src/ directory

**Values stay at zero**
- Normal - test uses simulated data
- Check for "Simulating discharge cycle" message

---

## Performance Benchmarks

These are expected values when running tests:

| Metric | Expected | Notes |
|--------|----------|-------|
| Free Heap | >400 KB | ESP32 has 520 KB total |
| CAN RX Rate | ~1000 msg/s | With sustained traffic |
| Logger Throughput | >500 msg/s | To SPIFFS |
| Boot Time | 2-3 seconds | From reset to "initialized" |
| Settings Load | <100 ms | From NVS |
| CAN Latency | <10 ms | RX to parsed data |

### Monitoring Performance

Watch serial output for:
```
Free heap: 425432 bytes  ✓ Good (>400KB)
Free heap: 250000 bytes  ⚠ Low, monitor
Free heap: 15000 bytes   ✗ Critical!

CAN Stats - Dropped: 0   ✓ No message loss
CAN Stats - Dropped: 50  ⚠ Increase queue size
```

---

## What You Can't Test Yet

❌ **Not implemented:**
- ACS712 current sensors (hardware + software needed)
- Voltage sensing (hardware + software needed)
- WiFi connectivity
- MQTT publishing
- Web dashboard
- WebSocket updates

These will be added in future modules.

---

## Next Steps After Testing

Once all tests pass:

1. **Add CAN hardware** for real message testing
2. **Reverse-engineer protocol** using can_test example
3. **Implement sensors** (next major module)
4. **Add networking** (WiFi, MQTT, web)
5. **Connect to real battery** (carefully!)

---

## Quick Test Checklist

Run through these to verify everything works:

- [ ] Main firmware builds without errors
- [ ] Main firmware uploads to ESP32
- [ ] Serial output shows startup banner
- [ ] Settings test passes all checks
- [ ] CAN test initializes successfully
- [ ] Battery test shows simulated data
- [ ] No "Guru Meditation" errors
- [ ] Free heap stays above 400 KB
- [ ] Can export CAN log (press 'e' in can_test)

If all checked, you're ready to proceed! ✅

---

## Getting Help

If tests fail, collect this info:
1. Serial output (entire startup sequence)
2. Which test is failing
3. Error messages
4. ESP32 board model
5. PlatformIO version (`pio --version`)

Check:
- BUILD_AND_TEST.md for detailed troubleshooting
- PROGRESS.md for implementation status
- Module README.md files for API details
