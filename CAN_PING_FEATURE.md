# CAN Bus Ping/Heartbeat Feature

## Overview

The CAN driver now includes a **ping/heartbeat** feature that periodically sends test messages to verify the CAN transceiver is working correctly. This is especially useful for:
- Testing CAN hardware connectivity
- Verifying the transceiver is operational
- Debugging CAN bus issues
- Confirming TX capability before receiving battery data

## Ping Message Format

**CAN ID**: `0x404`
**Data Length**: 8 bytes
**Pattern**: Alternating `F0F0...` pattern

The data pattern alternates on each ping:
- **Ping 1**: `F0 0F F0 0F F0 0F F0 0F`
- **Ping 2**: `0F F0 0F F0 0F F0 0F F0`
- **Ping 3**: `F0 0F F0 0F F0 0F F0 0F`
- And so on...

This alternating pattern makes it easy to visually identify ping messages in CAN logs and verify the transceiver is actively transmitting.

## Configuration

### Compile-Time Configuration

Edit `/workspace/src/config/config.h`:

```cpp
// CAN Configuration
#define CAN_PING_ENABLED    true    // Enable/disable periodic ping
#define CAN_PING_INTERVAL   1000    // Ping interval in milliseconds
#define CAN_PING_ID         0x404   // Ping message ID
```

**Default Settings:**
- **Enabled**: Yes (`true`)
- **Interval**: 1000 ms (1 second)
- **Message ID**: 0x404

### Disabling the Ping

To disable the ping feature:

**Option 1**: Edit `config.h` and set:
```cpp
#define CAN_PING_ENABLED    false
```

**Option 2**: Runtime disable (via code):
```cpp
canDriver.disablePeriodicPing();
```

## API Reference

### `bool sendPing()`
Sends a single ping message immediately.

**Returns**: `true` if sent successfully, `false` otherwise

**Example**:
```cpp
if (canDriver.sendPing()) {
    Serial.println("Ping sent!");
}
```

### `void enablePeriodicPing(uint32_t interval_ms)`
Enables automatic periodic ping messages.

**Parameters**:
- `interval_ms`: Time between pings in milliseconds

**Example**:
```cpp
canDriver.enablePeriodicPing(2000);  // Ping every 2 seconds
```

### `void disablePeriodicPing()`
Disables automatic periodic ping messages.

**Example**:
```cpp
canDriver.disablePeriodicPing();
```

## Where CAN Messages Are Read

The CAN bus message reception happens in **`/workspace/src/can/can_driver.cpp`**:

### Key Functions

**`processReceivedMessages()`** (Line 222-265)
- Polls the TWAI (CAN) driver for new messages
- Converts TWAI message format to internal `CANMessage` format
- Adds messages to the RX ring buffer
- Calls registered callbacks for each received message
- Monitors bus status for errors

**`rxTaskFunc()`** (Line 211-220)
- FreeRTOS task that runs continuously
- Calls `processReceivedMessages()` in a loop
- Runs on CPU core 0 with priority 2

### Message Flow

```
┌─────────────────────┐
│   CAN Transceiver   │ (TJA1050)
│     (Hardware)      │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│   TWAI Controller   │ (ESP32 hardware)
│   (ESP32 Builtin)   │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│  processReceived()  │ ← Called by RX Task
│  can_driver.cpp:222 │
└──────────┬──────────┘
           │
           ├──► RX Ring Buffer (100 messages)
           │
           └──► Message Callback (logging & parsing)
                      │
                      ├──► canLogger.logMessage()
                      └──► canParser.parseMessage()
```

## Monitoring Ping Messages

### Serial Monitor

When the ping is enabled, you'll see output like:
```
[CAN] CAN ping enabled (interval: 1000 ms, ID: 0x404)
[CAN] Ping sent: ID=0x404, counter=1
[CAN] Ping sent: ID=0x404, counter=2
[CAN] Ping sent: ID=0x404, counter=3
...
```

### Web Interface

1. Navigate to your ESP32's IP address
2. Click the **gear icon** to open Settings
3. Click on **API Endpoints** → `/api/canlog`
4. Look for messages with ID `0x404`

**Example JSON Response**:
```json
{
  "messages": [
    {
      "id": "0x404",
      "dlc": 8,
      "data": "F00FF00FF00FF00F",
      "timestamp": 1234567,
      "extended": false,
      "rtr": false
    }
  ]
}
```

### CAN Log Download

1. Open web interface
2. Navigate to `/api/canlog/download`
3. Download CSV file
4. Look for rows with ID `0x404`

**Example CSV**:
```csv
timestamp,id,dlc,data
1234567,0x404,8,F00FF00FF00FF00F
1235567,0x404,8,0FF00FF00FF00FF0
1236567,0x404,8,F00FF00FF00FF00F
```

## Troubleshooting

### Ping Messages Not Appearing

**Check Serial Output**:
```
[CAN] CAN ping enabled (interval: 1000 ms, ID: 0x404)
[CAN] Ping sent: ID=0x404, counter=1
```

If you see "Ping sent" but don't receive it:
1. **Check CAN transceiver wiring**:
   - TX: GPIO 5 → TJA1050 TXD
   - RX: GPIO 4 → TJA1050 RXD (via voltage divider)
   - CANH and CANL connected to bus
   - 120Ω termination resistor if needed

2. **Check for loopback mode**:
   - In `can_driver.cpp`, line 28 should be:
     ```cpp
     TWAI_MODE_NORMAL  // Not TWAI_MODE_LISTEN_ONLY
     ```

3. **Check CAN bus termination**:
   - Some CAN interfaces require 120Ω termination between CANH/CANL
   - ESP32 should see its own transmitted messages if termination is correct

### Ping Sent But "TX Failed"

Check serial for:
```
[CAN] Ping failed to send
```

**Possible causes**:
- TX queue full (increase `CAN_TX_QUEUE_SIZE` in config.h)
- Bus-off state (check for excessive errors)
- Hardware issue with transceiver

**Check TX statistics**:
```cpp
const CANStats& stats = canDriver.getStats();
Serial.printf("TX Count: %d, TX Failed: %d\n", stats.tx_count, stats.tx_failed);
```

### Bus-Off State

If you see:
```
[CAN] Bus-off detected!
[CAN] Attempting automatic recovery...
```

**Possible causes**:
- No other devices on the bus (CAN requires at least 2 nodes)
- Missing termination resistor
- Incorrect bitrate (should be 500 kbps)
- Wiring issues

**Solution**:
- Connect to an actual battery module or CAN analyzer
- Add 120Ω termination between CANH and CANL
- Verify all devices are at 500 kbps

## Integration with CAN Analyzer

If you're using a CAN analyzer (USB-CAN adapter, CANable, etc.):

### Linux (SocketCAN)
```bash
# Set up CAN interface at 500 kbps
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0

# Monitor for ping messages
candump can0 | grep 404
```

### Windows (PCAN, etc.)
Use PCAN-View or similar software:
1. Set bitrate to 500 kbps
2. Add filter for ID `0x404`
3. You should see messages every 1 second

## Use Cases

### 1. Initial Hardware Testing
When setting up the hardware for the first time:
```cpp
// main.cpp - temporary test code
void setup() {
    Serial.begin(115200);
    canDriver.begin(500000);
    canDriver.enablePeriodicPing(500);  // Fast ping for testing
}
```

### 2. Debugging "No Data" Issues
If battery data isn't appearing:
1. Check if ping messages are being logged
2. If ping works, the transceiver is OK
3. Issue is likely with battery module or protocol

### 3. Bus Activity Indicator
Use ping to keep the bus active:
```cpp
// Slow ping to maintain bus presence
canDriver.enablePeriodicPing(5000);  // Every 5 seconds
```

### 4. Testing Different Message IDs
Change the ping ID to test filters:
```cpp
// In config.h
#define CAN_PING_ID  0x123  // Test different ID
```

## Performance Impact

The ping feature has minimal performance impact:
- **CPU**: ~0.1% (single message every 1 second)
- **Memory**: 16 bytes (ping state variables)
- **CAN Bandwidth**: 0.016% at 500 kbps (one 8-byte message per second)

The ping is sent from the same task that receives messages, so there's no additional task overhead.

## Advanced Usage

### Conditional Ping Based on Battery Connection

```cpp
// Only ping if no battery data received
uint32_t last_battery_msg = 0;
const uint32_t BATTERY_TIMEOUT = 5000;

void loop() {
    if (millis() - last_battery_msg > BATTERY_TIMEOUT) {
        // No battery data, enable ping to test bus
        canDriver.enablePeriodicPing(1000);
    } else {
        // Battery is active, disable ping
        canDriver.disablePeriodicPing();
    }
}
```

### Using Ping Response for Self-Test

```cpp
uint32_t ping_sent_time = 0;
bool ping_received = false;

// In setup:
canDriver.setMessageCallback([](const CANMessage& msg) {
    if (msg.id == 0x404) {
        ping_received = true;
        Serial.println("✓ CAN loopback working!");
    }
});

canDriver.sendPing();
ping_sent_time = millis();

// Check after 100ms
delay(100);
if (ping_received) {
    Serial.println("CAN transceiver test: PASS");
} else {
    Serial.println("CAN transceiver test: FAIL - check wiring");
}
```

## Future Enhancements

Potential improvements:
- Variable data patterns (counter, timestamp)
- Ping response/echo from other devices
- Round-trip time (RTT) measurement
- Automatic bus health scoring
- Web UI toggle for ping enable/disable
