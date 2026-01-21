# CAN Bus BUS_OFF Issue - Root Cause and Fix

## What's Happening

Your error **"CAN TX timeout: TX queue full"** is a **symptom**, not the root cause. Here's what's happening:

```
1. ESP32 tries to send ping message
   ↓
2. TJA1050 transceiver transmits it on CAN bus
   ↓
3. NO OTHER DEVICE RESPONDS (no ACK bit)
   ↓
4. TWAI controller increments TX error counter
   ↓
5. After ~128 errors → BUS_OFF state
   ↓
6. All future TX attempts fail immediately
   ↓
7. Messages pile up in TX queue (max 20)
   ↓
8. Queue fills → "TX timeout: TX queue full"
```

## Root Cause: BUS_OFF State

The CAN controller goes into **BUS_OFF** state when it detects too many transmission errors. This happens because:

### Missing ACK (Acknowledgment)

CAN protocol requires **at least 2 devices** on the bus:
- Device A sends a message
- Device B must send an **ACK bit** to confirm reception
- If no ACK is received → error counter increments
- After 128+ errors → BUS_OFF

### Why No ACK?

One or more of these issues:

1. **No Termination Resistor** (most common)
   - CAN bus requires **120Ω resistor** at EACH end
   - Without it, signals reflect and corrupt the data
   - No ACK is received

2. **No Other CAN Device**
   - If ESP32 is alone on the bus, no one can send ACK
   - CAN requires minimum 2 nodes

3. **Wrong Bitrate**
   - ESP32: 500 kbps (default)
   - Battery: might be 250 kbps or 1000 kbps
   - If they don't match, no ACK

4. **Wiring Issues**
   - CANH/CANL swapped
   - Poor connections
   - Wrong voltage levels

## The Fix

### Fix 1: Add Termination Resistor (Most Important!)

**What you need**: 120Ω resistor (1/4 watt is fine)

**Where to connect**:
```
TJA1050 Pin 6 (CANH) ──┬── Battery CANH
                       │
                     [120Ω]
                       │
TJA1050 Pin 7 (CANL) ──┴── Battery CANL
```

**Why**: CAN bus is a transmission line. Without termination, signals reflect back and cause errors. The 120Ω resistor prevents reflections.

### Fix 2: Connect to Battery Module

Your ESP32 needs another CAN device to talk to. Options:

**Option A**: Connect to battery module
- Battery provides ACK when it receives messages
- ESP32 can also receive battery messages

**Option B**: Connect USB CAN adapter (for testing)
- PEAK CAN, CANable, PCAN-USB, etc.
- This confirms your hardware works

**Option C**: Two ESP32s (for testing)
- Connect two ESP32s together
- Each can ACK the other's messages

### Fix 3: Check/Change Bitrate

If your battery uses different bitrate, change in `config.h`:

```cpp
// Try these common CAN bitrates
#define CAN_BITRATE 500000  // 500 kbps (default)
//#define CAN_BITRATE 250000  // 250 kbps (common for batteries)
//#define CAN_BITRATE 1000000 // 1 Mbps (less common)
```

## Updated Firmware Features

I've updated the firmware to help debug this:

### 1. Periodic Bus State Logging

Every 10 seconds, the firmware logs the bus state:

**If BUS_OFF**:
```
[WARN] CAN Bus State: BUS_OFF, TX Errors: 255, RX Errors: 128, Queued: 20
```

**If RUNNING**:
```
[DEBUG] CAN Bus: RUNNING, TX:0 RX:0 Errors:TX=0,RX=0
```

### 2. Better Error Messages

**Before**:
```
[WARN] [CAN] Ping failed to send
```

**After** (tells you the problem):
```
[ERROR] CAN TX failed: invalid state (bus-off or not started)
[ERROR] CANDriver: Bus-off detected! TX errors=255, RX errors=128
[ERROR] This usually means: no termination resistor, no other CAN device, or wrong bitrate
```

### 3. Smart Ping (Only When Bus is Running)

Ping now only sends when bus is in RUNNING state. This prevents queue overflow when bus is BUS_OFF.

### 4. Ping Disabled by Default

I've disabled ping by default in `config.h`:

```cpp
#define CAN_PING_ENABLED false  // Disabled until you fix termination
```

**Why**: This prevents the TX queue from filling up while you debug the bus issue.

**Re-enable after fix**:
```cpp
#define CAN_PING_ENABLED true
```

## Testing Steps

### Step 1: Upload New Firmware

```bash
pio run --target upload
```

### Step 2: Check Logs

Visit: `http://<ESP32_IP>/api/logs`

Look for these messages:

**Bad** (needs fixing):
```json
{
  "level": "ERROR",
  "msg": "CANDriver: Bus-off detected! TX errors=255, RX errors=128"
}
```

**Good** (fixed!):
```json
{
  "level": "INFO",
  "msg": "CANDriver: Bus recovered to RUNNING state"
}
```

### Step 3: Check Diagnostics

Visit: `http://<ESP32_IP>/api/can/diagnostics`

**Before fix**:
```
CAN Driver Status:
  Status: BUS_OFF
  TX Failed: 156

TWAI Hardware:
  State: BUS_OFF
  TX Error Counter: 255
```

**After fix**:
```
CAN Driver Status:
  Status: RUNNING
  TX Count: 245
  TX Failed: 0

TWAI Hardware:
  State: RUNNING
  TX Error Counter: 0
  RX Error Counter: 0
```

### Step 4: Test Ping (After Bus is RUNNING)

Once the bus is in RUNNING state, re-enable ping:

1. Edit `/workspace/src/config/config.h`:
   ```cpp
   #define CAN_PING_ENABLED true
   ```

2. Rebuild and upload:
   ```bash
   pio run --target upload
   ```

3. Check logs for successful pings:
   ```
   [DEBUG] [CAN] Ping sent: ID=0x404, counter=1
   ```

4. If you have loopback (termination on both ends), you should see ping messages in `/api/canlog`

## Typical Hardware Setup

### Minimal Test Setup (ESP32 + Battery)

```
┌──────────────┐         ┌─────────────┐         ┌────────────────┐
│   ESP32      │         │  TJA1050    │         │ Battery Module │
│              │         │ Transceiver │         │                │
│ GPIO 5 (TX) ─┼────────→│ TXD   CANH  ├─────┬───┤ CANH           │
│ GPIO 4 (RX) ─┼←────────│ RXD   CANL  ├──┐  │   │ CANL           │
│              │         │             │  │  │   │                │
│ GND ─────────┼─────────│ GND         │  │  │   │ GND            │
│ 5V ──────────┼─────────│ VCC         │  │  │   │                │
└──────────────┘         └─────────────┘  │  │   └────────────────┘
                                          │  │
                                        [120Ω]  ← ADD THIS!
                                          │  │
                                          └──┘
```

**Important**:
- TJA1050 needs 5V power
- 120Ω termination resistor is CRITICAL
- GPIO 4 (RX) needs voltage divider (5V → 3.3V)

### Voltage Divider for RX (if not already added)

```
TJA1050 RXD (5V) ──┬──[10kΩ]──┬── ESP32 GPIO 4 (3.3V max)
                   │          │
                  [20kΩ]     GND
                   │
                  GND
```

This scales 5V to ~3.33V max, safe for ESP32.

## Expected Results

### After Adding Termination + Battery

1. **Bus state changes to RUNNING**:
   ```
   [INFO] CANDriver: Initialized successfully at 500000 bps
   [DEBUG] CAN Bus: RUNNING
   ```

2. **No more BUS_OFF errors**

3. **TX counter increases, TX Failed stays at 0**

4. **Messages from battery appear** in `/api/canlog`:
   ```json
   {"id": "0x201", "dlc": 8, "data": "..."}
   {"id": "0x202", "dlc": 8, "data": "..."}
   ```

### After Re-enabling Ping

1. **Pings sent successfully**:
   ```
   [DEBUG] [CAN] Ping sent: ID=0x404, counter=1
   ```

2. **Ping messages in log** (if you have loopback):
   ```
   Visit: /api/canlog
   Look for: ID 0x404
   ```

## Still Having Issues?

If the bus is still going BUS_OFF after adding termination:

1. **Check wiring**:
   - CANH to CANH (not swapped)
   - CANL to CANL
   - Good solder joints

2. **Try different bitrate**:
   - Change `CAN_BITRATE` to 250000
   - Some batteries use 250 kbps

3. **Check battery is powered on**:
   - Battery must be on to respond
   - Some batteries only TX when charging/discharging

4. **Verify transceiver voltage**:
   - TJA1050 VCC: should be 5V
   - Measure with multimeter

5. **Test with CAN analyzer**:
   - Connect USB CAN adapter
   - See if ESP32 messages appear
   - Confirms TX path works

## Summary

**The "TX queue full" error is NOT the problem** - it's a symptom.

**The real problem is BUS_OFF state** caused by:
- ❌ No termination resistor → **Add 120Ω**
- ❌ No other CAN device → **Connect battery**
- ❌ Wrong bitrate → **Check battery specs**

**After the fix, you should see**:
- ✅ Status: RUNNING
- ✅ TX errors: 0
- ✅ Messages from battery in logs
- ✅ No "TX timeout" errors

The updated firmware will help you see exactly when the bus goes BUS_OFF and why, making it much easier to diagnose.
