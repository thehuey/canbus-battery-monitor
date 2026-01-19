# CAN Bus Message Logging

## Overview

The eBike Battery Monitor includes a comprehensive CAN bus message logging system with automatic rolling log management.

## Features

### 1. **Dual-Buffer Architecture**
- **Memory Buffer**: Stores last 1,000 messages in RAM for instant web interface access
- **Write Buffer**: 100-message buffer for batch writing to SPIFFS (reduces wear)
- **SPIFFS Storage**: Persistent CSV log file on flash storage

### 2. **Automatic Rolling Log**
- Monitors SPIFFS usage continuously
- When storage reaches **80% full**, automatically rotates the log
- Rotation clears old data to make room for new messages
- Configurable via `SPIFFS_ROTATION_PERCENT` in config.h

### 3. **CSV Format**
Log file: `/canlog.csv`

Format:
```csv
Timestamp,ID,DLC,Data,Extended,RTR
1234567,0x123,8,01 02 03 04 05 06 07 08,0,0
```

Fields:
- **Timestamp**: Milliseconds since boot
- **ID**: CAN message ID (hex format)
- **DLC**: Data Length Code (0-8 bytes)
- **Data**: Hex bytes separated by spaces
- **Extended**: 1 for extended frame, 0 for standard
- **RTR**: 1 for Remote Transmission Request, 0 for data frame

### 4. **Auto-Flush**
- Buffered messages automatically flushed to SPIFFS every **5 seconds**
- Reduces file system writes and extends flash lifetime
- Configurable via `CAN_LOG_FLUSH_INTERVAL_MS` in config.h

## Web Interface Access

### View Recent Messages
```
GET /api/canlog
```

Query Parameters:
- `limit`: Max messages to return (default: 100, max: 1000)
- `filter`: CAN ID to filter by (e.g., `filter=0x123`)

Example:
```bash
# Get last 100 messages
curl http://192.168.4.1/api/canlog

# Get last 500 messages
curl http://192.168.4.1/api/canlog?limit=500

# Filter by CAN ID 0x123
curl http://192.168.4.1/api/canlog?filter=0x123
```

Response:
```json
{
  "messages": [
    {
      "id": "0x123",
      "dlc": 8,
      "data": "0102030405060708",
      "timestamp": 1234567,
      "extended": false
    }
  ],
  "count": 1,
  "total_logged": 5432,
  "dropped": 0
}
```

### Download Full Log as CSV
```
GET /api/canlog/download
```

Downloads the complete log file as `canlog.csv` attachment.

### Clear Log
```
POST /api/canlog/clear
```

Clears all logged messages and resets the log file.

## Configuration

### Adjust Log Size
Edit `src/config/config.h`:

```cpp
// Ring buffer size for in-memory access
#define CAN_LOG_MAX_ENTRIES     1000    // Increase for more memory buffer

// Rotation threshold
#define SPIFFS_ROTATION_PERCENT 80      // Rotate at 80% SPIFFS usage
```

### Adjust Flush Interval
```cpp
// How often to write buffered messages to SPIFFS
#define CAN_LOG_FLUSH_INTERVAL_MS  5000  // 5 seconds
```

### Disable Auto-Flush (Advanced)
```cpp
// In main.cpp or setup code
canLogger.setAutoFlush(false);
canLogger.setFlushInterval(10000);  // Manual flush interval
```

## Implementation Details

### File: `src/can/can_logger.cpp`

**Key Components:**

1. **logMessage()** - Adds message to buffers
   - Stores in memory buffer (for web interface)
   - Stores in write buffer (for SPIFFS)
   - Triggers auto-flush if interval elapsed

2. **flush()** - Writes buffered messages to SPIFFS
   - Opens log file in append mode
   - Writes all pending messages
   - Checks if rotation needed
   - Closes file

3. **checkAndRotate()** - Manages storage
   - Calculates SPIFFS usage percentage
   - Clears log when threshold reached
   - Prevents SPIFFS from filling up

4. **getRecentMessages()** - Web interface access
   - Returns messages from memory buffer
   - No file I/O required (fast)
   - Supports filtering by CAN ID

### Usage in Code

```cpp
// Initialize logger
canLogger.begin("/canlog.csv");

// Log a CAN message (automatic via callback)
canDriver.setMessageCallback([](const CANMessage& msg) {
    canLogger.logMessage(msg);
});

// Manual flush (called every 5s in canTask)
canLogger.flush();

// Get statistics
uint32_t total = canLogger.getMessageCount();
uint32_t dropped = canLogger.getDroppedCount();
size_t file_size = canLogger.getLogSize();
```

## Storage Considerations

### SPIFFS Capacity
- **NodeMCU-32S**: Typically 1.5 MB SPIFFS partition
- **Log Entry Size**: ~60 bytes per message (CSV format)
- **Approximate Capacity**: ~25,000 messages at 80% usage
- **Rotation**: Clears log when reaching capacity

### Message Rate Calculation
If receiving 100 CAN messages/second:
- **Storage fill rate**: ~6 KB/second
- **Time to 80% full**: ~3.5 hours with 1.5 MB partition
- **Auto-rotation**: Occurs automatically

### Extending Storage
To log for longer periods without rotation:
1. Use larger SPIFFS partition (edit partition table)
2. Implement external SD card logging (future enhancement)
3. Stream logs to MQTT or external server

## Troubleshooting

### Log Not Recording
1. Check serial output for "CANLogger: Initialized"
2. Verify SPIFFS mounted successfully
3. Check free space: SPIFFS.totalBytes() / SPIFFS.usedBytes()

### High Dropped Count
- Increase write buffer size: `WRITE_BUFFER_SIZE` in can_logger.h
- Reduce flush interval for more frequent writes
- Check for SPIFFS performance issues

### Rotation Too Frequent
- Decrease `SPIFFS_ROTATION_PERCENT` (e.g., 90%)
- Increase SPIFFS partition size
- Implement external logging

## Future Enhancements

- [ ] Configurable rotation strategies (keep recent N messages)
- [ ] Export to SD card
- [ ] MQTT log streaming
- [ ] Compression for archived logs
- [ ] Web-based log viewer with filtering
- [ ] Real-time CAN message visualization
