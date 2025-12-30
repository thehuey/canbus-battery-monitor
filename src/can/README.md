# CAN Module

This module implements CAN bus communication using ESP32's TWAI (Two-Wire Automotive Interface) controller. It includes a driver for sending/receiving messages, a parser for decoding battery data, and a logger for SPIFFS storage.

## Files

- `can_message.h` - CAN frame structures and battery data definitions
- `can_parser.h/cpp` - Protocol parser for extracting battery data from CAN messages
- `can_driver.h/cpp` - TWAI driver with message queuing and error handling
- `can_logger.h/cpp` - SPIFFS-based logging with CSV export

## Hardware Connection

The ESP32 TWAI controller connects to a CAN transceiver (TJA1050):

```
ESP32 GPIO 5 (TX) ──▶ TJA1050 TXD
ESP32 GPIO 4 (RX) ◀── TJA1050 RXD
                      TJA1050 CANH ──▶ CAN Bus H
                      TJA1050 CANL ──▶ CAN Bus L
```

**Important**: Use appropriate voltage dividers if the transceiver outputs 5V logic.

## CAN Driver API

### Initialization

```cpp
#include "can/can_driver.h"

void setup() {
    // Initialize at 500 kbps (default)
    if (!canDriver.begin(500000)) {
        Serial.println("CAN init failed!");
    }
}
```

### Sending Messages

```cpp
CANMessage msg;
msg.id = 0x123;
msg.dlc = 8;
msg.extended = false;
msg.rtr = false;
msg.data[0] = 0x01;
msg.data[1] = 0x02;
// ... set remaining data bytes

if (canDriver.sendMessage(msg)) {
    Serial.println("Message sent!");
}
```

### Receiving Messages

```cpp
CANMessage msg;

// Non-blocking receive
if (canDriver.receiveMessage(msg, 0)) {
    Serial.printf("Received ID: 0x%03X\n", msg.id);
}

// Blocking receive with timeout
if (canDriver.receiveMessage(msg, 1000)) {  // 1 second timeout
    Serial.printf("Received ID: 0x%03X\n", msg.id);
}
```

### Message Callback

```cpp
// Set up callback for all received messages
canDriver.setMessageCallback([](const CANMessage& msg) {
    Serial.printf("RX: ID=0x%03X, DLC=%d\n", msg.id, msg.dlc);
});
```

### Status and Statistics

```cpp
// Check driver status
CANStatus status = canDriver.getStatus();
if (status == CANStatus::BUS_OFF) {
    Serial.println("Bus-off detected!");
    canDriver.recoverBusOff();
}

// Get statistics
const CANStats& stats = canDriver.getStats();
Serial.printf("RX: %lu, TX: %lu, Errors: %lu\n",
             stats.rx_count, stats.tx_count, stats.error_count);
```

## CAN Parser API

The parser extracts battery data from CAN messages based on the protocol specification.

### Basic Parsing

```cpp
#include "can/can_parser.h"

CANParser parser;
CANMessage msg;
CANBatteryData battData;

if (canDriver.receiveMessage(msg)) {
    if (parser.parseMessage(msg, battData)) {
        Serial.printf("Battery %d: %.1fV, %.1fA, SOC=%d%%\n",
                     battData.battery_id,
                     battData.pack_voltage,
                     battData.pack_current,
                     battData.soc);
    }
}
```

### Custom Message Handlers

Register custom handlers for specific CAN IDs:

```cpp
bool myCustomHandler(const CANMessage& msg, CANBatteryData& data) {
    // Custom parsing logic
    data.battery_id = 0;
    data.pack_voltage = (msg.data[0] << 8 | msg.data[1]) * 0.01f;
    data.valid = true;
    return true;
}

// Register handler
parser.registerHandler(0x300, myCustomHandler);
```

### Built-in Parsers

The parser includes example parsers for:

- **0x100-0x104**: Battery status messages (voltage, current, SOC, temp)
- **0x200-0x204**: Cell voltage messages (placeholder)

Update `can_parser.cpp` as you reverse-engineer your specific battery protocol.

## CAN Logger API

The logger stores CAN messages to SPIFFS for analysis and provides in-memory buffering for the web interface.

### Initialization

```cpp
#include "can/can_logger.h"

void setup() {
    if (!canLogger.begin("/canlog.csv")) {
        Serial.println("Logger init failed!");
    }
}
```

### Logging Messages

```cpp
CANMessage msg;

if (canDriver.receiveMessage(msg)) {
    canLogger.logMessage(msg);  // Logs to buffer
}

// Manually flush to file
canLogger.flush();
```

### Auto-Flush Configuration

```cpp
canLogger.setAutoFlush(true);
canLogger.setFlushInterval(5000);  // Flush every 5 seconds
```

### Exporting Logs

```cpp
// Export all messages to serial
canLogger.exportCSV(Serial);

// Export filtered messages (ID 0x100 only)
canLogger.exportFiltered(Serial, 0x100);
```

### Retrieving Recent Messages

```cpp
CANMessage buffer[100];
size_t count;

// Get up to 100 recent messages
if (canLogger.getRecentMessages(buffer, count, 100)) {
    for (size_t i = 0; i < count; i++) {
        Serial.printf("ID: 0x%03X\n", buffer[i].id);
    }
}

// Get filtered messages
if (canLogger.getFilteredMessages(buffer, count, 100, 0x100)) {
    Serial.printf("Found %d messages with ID 0x100\n", count);
}
```

### Log Management

```cpp
// Get log file size
size_t size = canLogger.getLogSize();
Serial.printf("Log size: %d bytes\n", size);

// Clear log
canLogger.clear();

// Get statistics
Serial.printf("Total messages: %lu, Dropped: %lu\n",
             canLogger.getMessageCount(),
             canLogger.getDroppedCount());
```

## CSV Log Format

The logger writes messages in CSV format:

```
Timestamp,ID,DLC,Data,Extended,RTR
1234567,0x100,8,01 02 03 04 05 06 07 08,0,0
1234590,0x101,8,AA BB CC DD EE FF 00 11,0,0
```

Fields:
- **Timestamp**: Milliseconds since boot
- **ID**: CAN identifier (hex)
- **DLC**: Data length code (0-8)
- **Data**: Space-separated hex bytes
- **Extended**: 1 for extended frame, 0 for standard
- **RTR**: 1 for remote transmission request, 0 for data frame

## Message Structures

### CANMessage

```cpp
struct CANMessage {
    uint32_t id;           // CAN identifier
    uint8_t dlc;           // Data length (0-8)
    uint8_t data[8];       // Data bytes
    uint32_t timestamp;    // Timestamp (ms)
    bool extended;         // Extended frame
    bool rtr;              // Remote transmission request
};
```

### CANBatteryData

```cpp
struct CANBatteryData {
    uint8_t battery_id;    // Battery module ID
    float pack_voltage;    // Pack voltage (V)
    float pack_current;    // Pack current (A, signed)
    uint8_t soc;           // State of charge (0-100%)
    float temp1;           // Temperature 1 (°C)
    float temp2;           // Temperature 2 (°C)
    uint8_t status_flags;  // Status bits
    bool valid;            // Data is valid
};
```

### Status Flags

```cpp
namespace CANStatusFlags {
    constexpr uint8_t CHARGING       = 0x01;
    constexpr uint8_t DISCHARGING    = 0x02;
    constexpr uint8_t BALANCING      = 0x04;
    constexpr uint8_t TEMP_WARNING   = 0x08;
    constexpr uint8_t OVER_VOLTAGE   = 0x10;
    constexpr uint8_t UNDER_VOLTAGE  = 0x20;
    constexpr uint8_t OVER_CURRENT   = 0x40;
    constexpr uint8_t ERROR          = 0x80;
}
```

## Error Handling

The CAN driver includes automatic error recovery:

### Bus-Off Recovery

```cpp
CANStatus status = canDriver.getStatus();

if (status == CANStatus::BUS_OFF) {
    Serial.println("Bus-off detected, attempting recovery...");
    if (canDriver.recoverBusOff()) {
        Serial.println("Recovery successful");
    }
}
```

The driver will also attempt automatic recovery when bus-off is detected.

### Message Dropped

Messages can be dropped if:
- RX queue is full (100 messages default)
- TX queue is full (20 messages default)
- Logger write buffer is full (100 messages default)

Monitor statistics to detect dropped messages:

```cpp
const CANStats& stats = canDriver.getStats();
if (stats.rx_dropped > 0) {
    Serial.printf("Warning: %lu messages dropped\n", stats.rx_dropped);
}
```

## Protocol Reverse Engineering

To reverse-engineer your battery's CAN protocol:

1. **Log all traffic**: Connect to the battery and let the logger run
2. **Export to CSV**: Use `canLogger.exportCSV()` to download the log
3. **Analyze patterns**: Look for periodic messages, correlate with battery state
4. **Update parser**: Implement your protocol in `can_parser.cpp`

### Example Workflow

```cpp
// 1. Log everything
canDriver.setMessageCallback([](const CANMessage& msg) {
    canLogger.logMessage(msg);
});

// 2. After logging session, export
canLogger.exportCSV(Serial);  // Or export to web interface

// 3. Analyze in spreadsheet, identify patterns

// 4. Implement custom parser
bool myBatteryParser(const CANMessage& msg, CANBatteryData& data) {
    // Your discovered protocol here
    return true;
}

canParser.registerHandler(0x100, myBatteryParser);
```

## Performance

- **CAN Bitrate**: 500 kbps (default)
- **RX Queue**: 100 messages
- **TX Queue**: 20 messages
- **Logger Memory Buffer**: 1000 messages
- **Logger Write Buffer**: 100 messages
- **Auto-flush Interval**: 5 seconds (configurable)

## Memory Usage

- CANDriver: ~500 bytes + 100 message queue (~1.3 KB)
- CANParser: ~300 bytes
- CANLogger: ~14 KB (1000 message buffer)
- **Total**: ~16 KB

## Thread Safety

⚠️ **Important**:
- The CAN driver runs its own RX task on core 0
- Message callbacks execute in the RX task context
- Keep callbacks short and non-blocking
- Use queues/semaphores if communicating with other tasks

## Example Usage

See `/examples/can_test.cpp` for a complete working example.

## Troubleshooting

### No messages received

- Check physical connections (CANH, CANL, GND)
- Verify CAN bus termination (120Ω resistors)
- Check bitrate matches bus speed
- Verify transceiver power supply (5V for TJA1050)

### Bus-off errors

- Check for short circuits on CANH/CANL
- Verify proper termination
- Reduce cable length if possible
- Check for loose connections

### Messages dropped

- Increase queue sizes in `config.h`
- Reduce message callback processing time
- Increase flush interval for logger

## Future Enhancements

- Dynamic hardware filters
- DBC file support for protocol definition
- CAN-FD support (if hardware supports it)
- Message replay from log
