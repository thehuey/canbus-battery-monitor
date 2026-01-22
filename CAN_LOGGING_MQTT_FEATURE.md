# CAN Message Logging and MQTT Publishing Feature

## Overview

This feature adds configurable control over CAN message storage and MQTT publishing, allowing users to:
1. Enable/disable local SPIFFS logging of CAN messages
2. Enable/disable publishing CAN messages to MQTT broker
3. Reduce storage usage when not needed
4. Stream CAN messages to external systems via MQTT

## New Settings

### 1. `can_log_enabled` (boolean)
- **Default**: `true`
- **Description**: Controls local SPIFFS storage of CAN messages
- **When enabled**: CAN messages are logged to `/canlog.csv` on SPIFFS
- **When disabled**: CAN messages are NOT stored locally

### 2. `mqtt_canmsg_enabled` (boolean)
- **Default**: `false`
- **Description**: Controls MQTT publishing of CAN messages
- **When enabled**: Each received CAN message is published to MQTT topic `<prefix>/canmsg`
- **When disabled**: CAN messages are NOT published to MQTT

## Configuration

### Via API

**GET Current Settings**
```bash
curl http://<ESP32_IP>/api/config
```

Response includes:
```json
{
  "can_bitrate": 500000,
  "can_log_enabled": true,
  "mqtt_canmsg_enabled": false,
  ...
}
```

**Update Settings**
```bash
curl -X POST http://<ESP32_IP>/api/config \
  -H "Content-Type: application/json" \
  -d '{
    "can_log_enabled": false,
    "mqtt_canmsg_enabled": true
  }'
```

### Via Settings Manager (Code)
```cpp
Settings& settings = settingsManager.getSettings();

// Disable local logging
settings.can_log_enabled = false;

// Enable MQTT publishing
settings.mqtt_canmsg_enabled = true;

// Save to NVS
settingsManager.save();
```

## MQTT Topic Format

### Topic
```
<mqtt_topic_prefix>/canmsg
```

Example: `ebike/canmsg`

### Message Format
```json
{
  "id": "0x201",
  "dlc": 8,
  "extended": false,
  "rtr": false,
  "timestamp": 123456789,
  "data": ["12", "B7", "5E", "78", "00", "00", "00", "00"]
}
```

#### Fields
- `id`: CAN message ID in hex format (e.g., "0x201")
- `dlc`: Data Length Code (0-8)
- `extended`: Boolean - true for 29-bit extended ID, false for 11-bit standard ID
- `rtr`: Boolean - Remote Transmission Request flag
- `timestamp`: Milliseconds since boot (from ESP32 millis())
- `data`: Array of hex-encoded bytes (e.g., ["FF", "00", "A5"])

## Use Cases

### Case 1: Production Monitoring
**Scenario**: Monitor battery in production, need real-time CAN data in MQTT but don't want to fill up SPIFFS

**Configuration**:
```json
{
  "can_log_enabled": false,
  "mqtt_canmsg_enabled": true
}
```

**Result**:
- No local storage used
- All CAN messages stream to MQTT in real-time
- External system (Node-RED, Home Assistant, etc.) processes messages

### Case 2: Development/Debug
**Scenario**: Reverse-engineering battery protocol, need to capture all messages locally

**Configuration**:
```json
{
  "can_log_enabled": true,
  "mqtt_canmsg_enabled": false
}
```

**Result**:
- All CAN messages saved to SPIFFS
- Can download via `/api/canlog/download`
- No MQTT traffic generated

### Case 3: Full Logging
**Scenario**: Maximum observability - log locally AND publish to MQTT

**Configuration**:
```json
{
  "can_log_enabled": true,
  "mqtt_canmsg_enabled": true
}
```

**Result**:
- CAN messages stored locally on SPIFFS
- CAN messages published to MQTT
- Both logging and real-time streaming

### Case 4: Minimal Operation
**Scenario**: Only parse battery data, don't need raw CAN logs

**Configuration**:
```json
{
  "can_log_enabled": false,
  "mqtt_canmsg_enabled": false
}
```

**Result**:
- No local logging
- No MQTT publishing
- Battery data still parsed and available via `/api/batteries`
- Minimal storage and network usage

## Implementation Details

### Data Flow

```
CAN Bus Message Received
        ↓
CANDriver (processReceivedMessages)
        ↓
Message Callback in main.cpp
        ↓
    ┌───────────────────────────┐
    │  Check Settings           │
    └───────────┬───────────────┘
                │
        ┌───────┴────────┐
        │                │
        ↓                ↓
  [can_log_enabled]  [mqtt_canmsg_enabled]
        │                │
        ↓                ↓
  canLogger.log()   mqttClient.publish()
        │                │
        ↓                ↓
   SPIFFS Storage    MQTT Broker
```

### Files Modified

1. **Settings Structure** (`src/config/settings.h`)
   - Added `bool can_log_enabled`
   - Added `bool mqtt_canmsg_enabled`

2. **Settings Implementation** (`src/config/settings.cpp`)
   - Added loading from NVS
   - Added saving to NVS
   - Added default values

3. **MQTT Client** (`src/network/mqtt_client.h/.cpp`)
   - Added `publishCANMessage(const CANMessage& msg)` method
   - Checks `mqtt_canmsg_enabled` setting before publishing
   - Formats message as JSON and publishes to `<prefix>/canmsg` topic

4. **Main Application** (`src/main.cpp`)
   - Updated CAN message callback to check settings
   - Conditionally calls `canLogger.logMessage()` if `can_log_enabled`
   - Conditionally calls `mqttClient.publishCANMessage()` if `mqtt_canmsg_enabled`

5. **Web Server** (`src/network/web_server.cpp`)
   - Added settings to `buildConfigJSON()` for GET `/api/config`
   - Added handling in `handlePostConfig()` for POST `/api/config`

## Performance Considerations

### SPIFFS Write Performance
- Local logging performs buffered writes
- Auto-flush every 5 seconds or on buffer full
- Minimal impact on CAN reception (~1-2% CPU)

### MQTT Publish Performance
- Each CAN message generates one MQTT publish
- At 500 kbps CAN with 50% bus utilization: ~60 messages/second
- MQTT QoS 0 (fire-and-forget) for minimal latency
- Network buffering handles burst traffic

### Storage Impact
- Local logging: ~50 bytes per message
- 100,000 messages = ~5 MB
- SPIFFS rotation at 80% capacity

### Network Impact
- JSON-encoded message: ~150 bytes
- At 60 msg/sec: ~9 KB/sec = 72 kbps
- WiFi easily handles this load

## Example MQTT Subscriber (Python)

```python
import paho.mqtt.client as mqtt
import json

def on_message(client, userdata, msg):
    data = json.loads(msg.payload)
    print(f"CAN Message: ID={data['id']} Data={data['data']}")

client = mqtt.Client()
client.on_message = on_message
client.connect("broker.example.com", 1883, 60)
client.subscribe("ebike/canmsg")
client.loop_forever()
```

## Example Node-RED Flow

```json
[
    {
        "id": "mqtt_in",
        "type": "mqtt in",
        "topic": "ebike/canmsg",
        "broker": "mqtt_broker"
    },
    {
        "id": "json_parse",
        "type": "json"
    },
    {
        "id": "filter_0x201",
        "type": "switch",
        "property": "payload.id",
        "rules": [{"t": "eq", "v": "0x201"}]
    },
    {
        "id": "debug",
        "type": "debug"
    }
]
```

## Troubleshooting

### Issue: MQTT messages not being published
**Check**:
1. `mqtt_canmsg_enabled` is `true` in settings
2. MQTT broker is connected (check `/api/status`)
3. Check MQTT broker logs for incoming connections
4. Verify topic subscription matches `<prefix>/canmsg`

### Issue: SPIFFS full
**Solution**:
1. Disable local logging: `can_log_enabled = false`
2. Clear existing log: POST to `/api/canlog/clear`
3. Or download and clear manually

### Issue: High network usage
**Solution**:
1. Disable MQTT CAN publishing: `mqtt_canmsg_enabled = false`
2. Or use MQTT filtering on broker side
3. Or reduce CAN bus traffic (if possible)

## Future Enhancements

Potential additions:
- MQTT QoS configuration (0, 1, or 2)
- CAN message filtering (publish only specific IDs)
- Compression for MQTT payloads
- Batch publishing (multiple messages per MQTT publish)
- Rate limiting for MQTT publishing
- Separate topic per CAN ID (e.g., `ebike/canmsg/0x201`)
