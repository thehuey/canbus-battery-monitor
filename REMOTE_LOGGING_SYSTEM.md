# Remote Logging System

## Overview

All `Serial.println()` and `Serial.printf()` calls have been replaced with the remote logging system (`LOG_INFO`, `LOG_WARN`, `LOG_DEBUG`, `LOG_ERROR`). This allows you to view logs from the web interface at `/api/logs` instead of only through the Serial monitor.

## Changes Made

### Files Updated

The following files have been updated to use the remote logging system:

1. **`src/can/can_driver.cpp`**
   - CAN initialization, shutdown, and error messages
   - Bus-off recovery messages
   - Ping/heartbeat messages

2. **`src/network/mqtt_client.cpp`**
   - MQTT connection status
   - Publish success/failure
   - TLS configuration

3. **`src/network/wifi_manager.cpp`**
   - WiFi connection status
   - AP mode setup
   - STA mode connection
   - Reconnection attempts

4. **`src/network/web_server.cpp`**
   - Server startup/shutdown
   - WebSocket connections (DEBUG level)
   - API endpoint registration

### Log Levels Used

**LOG_DEBUG** - Verbose debugging information
- WebSocket client connections/disconnections
- CAN ping messages sent
- Detailed state changes

**LOG_INFO** - Informational messages (default level)
- System initialization
- Successful operations
- Status updates
- Configuration changes

**LOG_WARN** - Warnings (things that might be problems)
- Connection failures (will retry)
- Unsupported configurations (falling back to defaults)
- Bus-off recovery attempts

**LOG_ERROR** - Errors (things that failed)
- Failed to initialize hardware
- Critical connection failures
- Bus-off recovery failed

## Viewing Logs

### Web Interface

**View Recent Logs**:
```
http://<ESP32_IP>/api/logs
```

**Example Response**:
```json
{
  "logs": [
    {
      "ts": 1234567,
      "level": "INFO",
      "msg": "CANDriver: Initialized successfully at 500000 bps"
    },
    {
      "ts": 1234580,
      "level": "INFO",
      "msg": "[CAN] Periodic ping enabled (interval: 1000 ms)"
    },
    {
      "ts": 1235567,
      "level": "DEBUG",
      "msg": "[CAN] Ping sent: ID=0x404, counter=1"
    }
  ],
  "count": 3,
  "buffer_size": 50
}
```

**Query Parameters**:
- `limit=N` - Limit to N most recent messages (max 50)
  ```
  /api/logs?limit=10
  ```

### Web Dashboard

Open the web interface and navigate to:
1. Click the **gear icon** (Settings)
2. Scroll to **API Endpoints**
3. Click on `/api/logs`

The logs will open in a new tab showing the JSON response.

### Serial Monitor (Still Works!)

Logs are still output to the Serial monitor at 115200 baud:
```bash
pio device monitor --baud 115200
```

Output format:
```
[INFO] CANDriver: Initialized successfully at 500000 bps
[INFO] [CAN] Periodic ping enabled (interval: 1000 ms)
[DEBUG] [CAN] Ping sent: ID=0x404, counter=1
[WARN] CANDriver: Attempting bus-off recovery...
[ERROR] CANDriver: Bus-off detected!
```

## Log Buffer

- **Buffer Size**: 50 messages (configurable in `remote_log.h`)
- **Storage**: Ring buffer in RAM (oldest messages are overwritten)
- **Thread Safe**: Uses FreeRTOS mutex for concurrent access

## Configuration

### Change Buffer Size

Edit `/workspace/src/utils/remote_log.h`:

```cpp
#define LOG_BUFFER_SIZE 100  // Increase to 100 messages
```

### Change Minimum Remote Log Level

By default, all log levels (DEBUG, INFO, WARN, ERROR) are stored in the buffer and available via `/api/logs`.

To filter what gets stored remotely (e.g., only INFO and above):

```cpp
// In main.cpp setup()
remoteLog.setRemoteLevel(LogLevel::INFO);  // Don't store DEBUG messages remotely
```

### Disable Serial Output

If you want logs only available via web (not Serial):

```cpp
// In main.cpp setup()
remoteLog.setSerialEnabled(false);
```

## Log Message Format

### Macros Available

```cpp
LOG_DEBUG("Debug message: %d", value);
LOG_INFO("Info message: %s", string);
LOG_WARN("Warning message");
LOG_ERROR("Error: code %d", error_code);
```

### Best Practices

1. **Use appropriate levels**:
   - DEBUG: Verbose, not needed for normal operation
   - INFO: General status and operations
   - WARN: Problems that don't stop operation
   - ERROR: Critical failures

2. **Include context**:
   ```cpp
   LOG_INFO("[CAN] Message received: ID=0x%03X", msg.id);
   ```

3. **No newlines needed**:
   ```cpp
   LOG_INFO("Message");  // Newline added automatically
   ```

4. **Keep messages under 128 characters** (buffer limit per message)

## Example Usage

### Adding Logs to Your Code

```cpp
#include "utils/remote_log.h"

void myFunction() {
    LOG_INFO("Starting operation");

    if (!initialize()) {
        LOG_ERROR("Failed to initialize");
        return;
    }

    LOG_DEBUG("Intermediate step: value=%d", someValue);

    if (result < threshold) {
        LOG_WARN("Result below threshold: %f < %f", result, threshold);
    }

    LOG_INFO("Operation completed successfully");
}
```

### Viewing Logs in Real-Time

Currently, logs are available via HTTP GET. To see logs in real-time, you can:

**Option 1**: Poll the endpoint
```javascript
setInterval(async () => {
    const response = await fetch('/api/logs?limit=10');
    const data = await response.json();
    console.log(data.logs);
}, 1000);
```

**Option 2**: Use the existing `/logs` live viewer page
- Navigate to `http://<ESP32_IP>/logs`
- This page has an embedded log viewer that auto-updates

## Integration with Home Assistant

```yaml
sensor:
  - platform: rest
    name: "eBike Recent Logs"
    resource: "http://192.168.1.100/api/logs?limit=5"
    json_attributes_path: "$.logs[0]"
    json_attributes:
      - level
      - msg
      - ts
    value_template: "{{ value_json.count }}"
```

## Comparison: Before vs After

### Before
```cpp
Serial.println("CANDriver: Initialized successfully");
Serial.printf("CAN bitrate: %d bps\n", bitrate);
```

**Problems**:
- Only visible via Serial cable
- No remote access
- Lost when buffer scrolls
- No log levels

### After
```cpp
LOG_INFO("CANDriver: Initialized successfully at %d bps", bitrate);
```

**Benefits**:
- ✅ Visible via web interface
- ✅ Stored in ring buffer
- ✅ Log levels for filtering
- ✅ Still output to Serial
- ✅ Thread-safe
- ✅ Timestamped

## Troubleshooting

### No Logs Appearing in /api/logs

1. **Check if remoteLog is initialized**:
   - Look for `remoteLog.begin()` in `main.cpp` setup()
   - Should be called early in startup

2. **Verify the endpoint exists**:
   ```bash
   curl http://<ESP32_IP>/api/logs
   ```

3. **Check Serial output**:
   - Logs should still appear on Serial
   - If Serial works but web doesn't, buffer might be full

### Logs Disappearing Quickly

The buffer only holds 50 messages. Increase `LOG_BUFFER_SIZE` if needed:

```cpp
// remote_log.h
#define LOG_BUFFER_SIZE 100
```

**Note**: Larger buffer uses more RAM.

### Too Much Debug Spam

Filter out DEBUG messages:

```cpp
// In main.cpp setup()
remoteLog.setRemoteLevel(LogLevel::INFO);
```

Or change verbose DEBUG calls to LOG_DEBUG so they can be filtered.

### Want Logs Pushed via WebSocket

Future enhancement - currently logs are pull-only (HTTP GET). To add push:

```cpp
// In web_server.cpp begin()
remoteLog.setBroadcastCallback([this](const LogEntry& entry) {
    JsonDocument doc;
    doc["type"] = "log";
    doc["ts"] = entry.timestamp;
    doc["level"] = RemoteLogger::levelToString(entry.level);
    doc["msg"] = entry.message;

    String json;
    serializeJson(doc, json);
    ws_.textAll(json);
});
```

## Files Reference

### Core Remote Logging Files

- **`src/utils/remote_log.h`** - RemoteLogger class and LOG_* macros
- **`src/utils/remote_log.cpp`** - Implementation

### Files Now Using Remote Logging

All major system files now use the logging system:
- CAN driver
- MQTT client
- WiFi manager
- Web server
- Main application

### Files Still Using Serial.print (Intentionally)

- **`src/utils/remote_log.cpp`** - Uses Serial internally to output logs
- **README.md files** - Documentation only

## Future Enhancements

Potential improvements:
- [ ] WebSocket broadcast of new log messages
- [ ] Log filtering by level in web UI
- [ ] Persistent log storage to SPIFFS
- [ ] Downloadable log file
- [ ] Colored log levels in web UI
- [ ] Search/filter logs by text
- [ ] Export logs as CSV
