# CAN Bus Troubleshooting Guide

## "Ping Failed to Send" Error

If you're seeing "Ping failed to send" errors in the logs, this guide will help you diagnose the issue.

## Quick Diagnostics

### 1. Check CAN Diagnostics Endpoint

Visit: **`http://<ESP32_IP>/api/can/diagnostics`**

This will show you detailed CAN bus status:

```
CAN Driver Status:
  Initialized: Yes
  Status: BUS_OFF
  Bitrate: 500000 bps

Statistics:
  RX Count: 0
  TX Count: 0
  RX Dropped: 0
  TX Failed: 156
  Bus-off Count: 3
  Error Count: 3

TWAI Hardware:
  State: BUS_OFF
  TX Queue: 0 messages waiting
  RX Queue: 0 messages waiting
  TX Error Counter: 255
  RX Error Counter: 127
  Bus Error Counter: 128
```

### 2. Check Recent Logs

Visit: **`http://<ESP32_IP>/api/logs`**

Look for error messages that explain why TX is failing:
- `"CAN TX timeout: TX queue full"`
- `"CAN TX failed: invalid state (bus-off)"`
- `"CAN TX failed: driver not initialized"`

## Common Issues and Solutions

### Issue 1: BUS_OFF State

**Symptoms**:
```
Status: BUS_OFF
TX Error Counter: 255
Error Count: > 0
```

**Cause**: The CAN controller has detected too many errors and shut down the bus. This usually means:
- No other devices on the bus (CAN requires at least 2 nodes)
- No termination resistor (120Ω required at each end of bus)
- Wrong bitrate (must match other devices - default is 500 kbps)
- Wiring issues (CANH/CANL swapped or shorted)

**Solutions**:

1. **Add Termination Resistor**:
   - Connect 120Ω resistor between CANH and CANL at EACH end of the bus
   - If ESP32 is the only device, you still need termination

2. **Connect to Another CAN Device**:
   - Battery module
   - USB CAN adapter
   - Another ESP32 with CAN

3. **Check Wiring**:
   ```
   TJA1050 Pin 6 (CANH) → Battery CANH (or CAN bus CANH)
   TJA1050 Pin 7 (CANL) → Battery CANL (or CAN bus CANL)
   TJA1050 Pin 1 (TXD)  ← ESP32 GPIO 5
   TJA1050 Pin 4 (RXD)  → ESP32 GPIO 4 (via voltage divider!)
   ```

4. **Verify Bitrate Matches**:
   - ESP32 default: 500 kbps
   - Battery module: Check specs (usually 250 or 500 kbps)
   - Change in `config.h` if needed:
     ```cpp
     #define CAN_BITRATE 250000  // Change to 250 kbps
     ```

5. **Manual Recovery** (if auto-recovery fails):
   ```cpp
   // Add this to your code temporarily for testing
   canDriver.recoverBusOff();
   ```

### Issue 2: TX Queue Full

**Symptoms**:
```
TX Failed: increasing rapidly
Logs show: "CAN TX timeout: TX queue full"
```

**Cause**: Trying to send messages faster than they can be transmitted.

**Solutions**:

1. **Increase TX Queue Size**:
   Edit `/workspace/src/config/config.h`:
   ```cpp
   #define CAN_TX_QUEUE_SIZE 50  // Increase from 20 to 50
   ```

2. **Reduce Ping Frequency**:
   Edit `/workspace/src/config/config.h`:
   ```cpp
   #define CAN_PING_INTERVAL 2000  // Increase from 1000ms to 2000ms
   ```

3. **Disable Ping Temporarily** (for testing):
   Edit `/workspace/src/config/config.h`:
   ```cpp
   #define CAN_PING_ENABLED false  // Disable ping
   ```

### Issue 3: Driver Not Initialized

**Symptoms**:
```
Logs show: "CAN TX failed: driver not initialized"
Initialized: No
```

**Cause**: CAN driver initialization failed during startup.

**Solutions**:

1. **Check Serial Logs** for initialization errors:
   ```
   [ERROR] CANDriver: Failed to install driver: 259
   [ERROR] CANDriver: Failed to start driver: 263
   ```

2. **Verify GPIO Pins** aren't already in use:
   - GPIO 5 (TX): Check not used by other peripherals
   - GPIO 4 (RX): Check not used by other peripherals

3. **Check TJA1050 Power**:
   - VCC: 5V
   - GND: Connected
   - Measure voltage on VCC pin

### Issue 4: Hardware State is STOPPED

**Symptoms**:
```
TWAI Hardware:
  State: STOPPED
```

**Cause**: Driver started but then stopped.

**Solutions**:

1. **Reboot the ESP32**
2. **Check for bus errors** in the diagnostics
3. **Enable more detailed logging** in Serial monitor

## Testing Steps

### Step 1: Loopback Test (No External CAN Device)

To test if your transceiver is working, add a **120Ω resistor between CANH and CANL** on the TJA1050. This creates a minimal CAN bus.

**Expected Result**: With termination, the ESP32 should receive its own ping messages (CAN echo).

**Check**:
```
Visit: http://<ESP32_IP>/api/canlog
Look for: messages with ID 0x404
```

If you see ping messages in the log, your transceiver TX and RX are working!

### Step 2: Connect USB CAN Adapter

If you have a USB CAN adapter (CANable, PCAN, etc.):

**Linux (SocketCAN)**:
```bash
# Set up CAN interface
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0

# Monitor for ESP32 pings
candump can0 | grep 404

# Send a test message TO the ESP32
cansend can0 123#0102030405060708
```

**Check ESP32 logs**:
```
Visit: http://<ESP32_IP>/api/logs
Look for: "CAN Driver Status: RUNNING"
```

### Step 3: Connect to Battery Module

**Prerequisites**:
- Battery module CAN bitrate matches (usually 500 kbps)
- 120Ω termination at ESP32 end
- 120Ω termination at battery end (may be built-in)

**Expected**:
- ESP32 should receive battery messages
- CAN log should show IDs from battery (e.g., 0x201-0x204)

## Enhanced Logging (Temporary Debug)

To see WHY messages are failing, the updated firmware now logs specific errors:

**Before** (not helpful):
```
[WARN] [CAN] Ping failed to send
```

**After** (helpful):
```
[ERROR] CAN TX timeout: TX queue full (increase CAN_TX_QUEUE_SIZE)
```

or

```
[ERROR] CAN TX failed: invalid state (bus-off or not started)
```

or

```
[ERROR] CAN TX failed: bus status is 2 (not RUNNING)
```

## Monitoring CAN Status

### Real-time Monitoring Script

Save this as `monitor_can.sh`:

```bash
#!/bin/bash
ESP32_IP="192.168.1.100"  # Change to your ESP32 IP

echo "=== CAN Diagnostics ==="
curl -s "http://$ESP32_IP/api/can/diagnostics"

echo -e "\n\n=== Recent Errors ==="
curl -s "http://$ESP32_IP/api/logs" | jq -r '.logs[] | select(.level=="ERROR" or .level=="WARN") | .msg'

echo -e "\n\n=== CAN Message Count ==="
curl -s "http://$ESP32_IP/api/status" | jq '.system.can_message_count'
```

Run:
```bash
chmod +x monitor_can.sh
./monitor_can.sh
```

### Watch CAN Stats

```bash
# Continuously monitor
watch -n 1 'curl -s http://192.168.1.100/api/can/diagnostics'
```

## Reference: Error Codes

### ESP-IDF Error Codes (esp_err_t)

Common errors from `twai_transmit()`:

| Code | Name | Meaning |
|------|------|---------|
| 0 | ESP_OK | Success |
| 259 | ESP_ERR_INVALID_ARG | Invalid parameter |
| 263 | ESP_ERR_INVALID_STATE | Bus not running or bus-off |
| 264 | ESP_ERR_TIMEOUT | TX queue full (10ms timeout) |

### TWAI States

| State | Meaning | Action |
|-------|---------|--------|
| RUNNING | Normal operation | ✅ Good |
| STOPPED | Driver stopped | Check initialization |
| BUS_OFF | Too many errors | Add termination, check wiring |
| RECOVERING | Recovering from bus-off | Wait or manual recover |

### Error Counters

- **TX Error Counter**:
  - 0-127: Normal
  - 128-255: Error-passive (warnings)
  - 255+: Bus-off (stopped)

- **RX Error Counter**:
  - 0-127: Normal
  - 128-255: Error-passive

**Rule**: If either counter reaches 255, controller goes BUS_OFF.

## Quick Fixes Summary

| Problem | Quick Fix |
|---------|-----------|
| BUS_OFF state | Add 120Ω termination resistor |
| TX queue full | Increase `CAN_TX_QUEUE_SIZE` to 50 |
| Too many pings | Increase `CAN_PING_INTERVAL` to 2000 |
| No messages | Connect to another CAN device |
| Still failing | Disable ping and test with battery only |

## Next Steps After Fix

Once you see:
```
Status: RUNNING
TX Failed: 0
TX Count: increasing
```

Then:
1. **Disable or slow down ping** (once you know CAN works)
2. **Connect battery module**
3. **Watch for battery messages** in `/api/canlog`
4. **Configure protocol** to parse battery data

## Need More Help?

**Collect this info**:
1. Output of `/api/can/diagnostics`
2. Output of `/api/logs?limit=50`
3. Your hardware setup (ESP32 model, transceiver type)
4. What devices are connected to the CAN bus
5. Whether you have termination resistors (120Ω)

This will help diagnose the specific issue with your setup.
