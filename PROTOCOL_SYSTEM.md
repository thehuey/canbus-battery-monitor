# Configurable CAN Protocol System - Implementation Summary

## Overview

The eBike Battery CANBUS Monitor now supports configurable CAN protocols, allowing you to use different battery modules without firmware recompilation. This system combines built-in protocols (compiled into firmware) with the ability to load, fetch, and store custom protocols.

## Architecture

### Storage Strategy

1. **Built-in Protocols** (Flash - compiled)
   - D-power 48V 13S (Protocol ID: 0)
   - Generic BMS (Protocol ID: 1)
   - Zero runtime overhead
   - Always available

2. **Custom Protocols** (SPIFFS)
   - JSON files stored in `/protocols/` directory
   - Fetched from URLs or uploaded via web interface
   - Persistent until explicitly deleted
   - No expiration or TTL

3. **Protocol Selection** (NVS)
   - Per-battery configuration
   - Stored in `BatteryConfig.protocol_source`
   - References either built-in or custom protocol path

## File Structure

```
/workspace/
├── data/
│   └── protocols/
│       ├── dpower_48v_13s.json         # Example protocol (reference)
│       ├── generic_bms.json            # Example protocol (reference)
│       └── SCHEMA.md                   # Protocol JSON schema docs
├── src/
│   ├── can/
│   │   ├── protocol.h                  # Protocol data structures
│   │   ├── protocol.cpp                # Protocol implementation
│   │   ├── builtin_protocols.h         # Built-in protocol declarations
│   │   ├── builtin_protocols.cpp       # Built-in protocol definitions
│   │   ├── protocol_loader.h           # JSON protocol loader
│   │   ├── protocol_loader.cpp         # Loader implementation
│   │   ├── can_parser.h                # Updated CAN parser (protocol-aware)
│   │   └── can_parser.cpp              # Parser implementation
│   ├── config/
│   │   ├── settings.h                  # Updated with protocol config
│   │   └── settings.cpp                # Load/save protocol settings
│   └── network/
│       ├── web_server.h                # Updated with protocol endpoints
│       └── protocol_api.cpp            # Protocol API handlers
```

## Protocol JSON Schema

### Basic Structure

```json
{
  "name": "Protocol Name",
  "manufacturer": "Manufacturer",
  "version": "1.0",
  "description": "Description",
  "cell_count": 13,
  "nominal_voltage": 48.0,
  "capacity_ah": 25.0,
  "chemistry": "Li-ion",
  "messages": [...]
}
```

### Message Definition

```json
{
  "can_id": 514,
  "can_id_hex": "0x202",
  "name": "Total Pack Voltage",
  "description": "Sum of all cell voltages",
  "period_ms": 100,
  "fields": [...]
}
```

### Field Definition

```json
{
  "name": "total_voltage_mv",
  "description": "Total pack voltage",
  "byte_offset": 0,
  "length": 2,
  "data_type": "uint16_le",
  "unit": "mV",
  "scale": 1.0,
  "offset": 0.0,
  "min_value": 39000,
  "max_value": 54600
}
```

### Supported Data Types

- `uint8`, `int8` (1 byte)
- `uint16_le`, `uint16_be`, `int16_le`, `int16_be` (2 bytes)
- `uint32_le`, `uint32_be`, `int32_le`, `int32_be` (4 bytes)
- `float_le`, `float_be` (4 bytes, IEEE 754)

### Value Calculation

```
physical_value = (raw_value × scale) + offset
```

Example: Average cell voltage from total pack voltage
```json
{
  "name": "avg_cell_voltage_mv",
  "byte_offset": 0,
  "length": 2,
  "data_type": "uint16_le",
  "scale": 0.07692307692,  // 1/13 cells
  "offset": 0.0
}
```

## Web API Endpoints

### List All Protocols
```
GET /api/protocols
```
Returns both built-in and custom protocols.

**Response:**
```json
{
  "builtin": [
    {
      "id": 0,
      "name": "Tianjin D-power 48V 13S",
      "manufacturer": "D-power",
      "version": "1.0",
      "source": "builtin"
    }
  ],
  "custom": [
    {
      "id": 0,
      "filename": "custom_0.json",
      "name": "My Custom Protocol",
      "manufacturer": "Custom",
      "size": 2048,
      "source": "custom"
    }
  ]
}
```

### List Built-in Protocols
```
GET /api/protocols/builtin
```

### List Custom Protocols
```
GET /api/protocols/custom
```

### Get Specific Protocol
```
GET /api/protocols/:id
```
- For built-in: `/api/protocols/builtin_0`
- For custom: `/api/protocols/custom_0` or `/api/protocols/custom_0.json`

### Upload Protocol
```
POST /api/protocols/upload
Content-Type: application/json

{
  "name": "My Battery Protocol",
  "manufacturer": "MyBrand",
  ...
}
```

**Response:**
```json
{
  "success": true,
  "filename": "/protocols/custom_0.json",
  "name": "My Battery Protocol"
}
```

### Fetch Protocol from URL
```
POST /api/protocols/fetch
Content-Type: application/json

{
  "url": "https://example.com/protocols/my_protocol.json"
}
```

**Response:**
```json
{
  "success": true,
  "filename": "/protocols/custom_1.json",
  "name": "Fetched Protocol",
  "source_url": "https://example.com/protocols/my_protocol.json"
}
```

### Delete Custom Protocol
```
DELETE /api/protocols/:id
```

**Response:**
```json
{
  "success": true,
  "message": "Protocol deleted"
}
```

### Validate Protocol
```
GET /api/protocols/:id/validate
```

**Response:**
```json
{
  "valid": true,
  "name": "Protocol Name",
  "message_count": 3
}
```

## Usage in Code

### Setting Protocol for Battery

```cpp
#include "can/builtin_protocols.h"
#include "can/protocol_loader.h"
#include "can/can_parser.h"

// Create protocol loader
Protocol::Loader loader;
loader.begin();

// Create CAN parser
CANParser parser;

// Option 1: Use built-in protocol
const Protocol::Definition* dpower = Protocol::getBuiltinProtocol(
    Protocol::BuiltinId::DPOWER_48V_13S);
parser.setProtocol(dpower);

// Option 2: Load custom protocol from SPIFFS
Protocol::Definition custom_proto;
if (loader.loadFromFile("/protocols/custom_0.json", custom_proto)) {
    parser.setProtocol(&custom_proto);
}

// Parse CAN messages
CANMessage msg;
// ... receive CAN message ...

CANBatteryData data;
if (parser.parseMessage(msg, data)) {
    Serial.printf("Pack voltage: %.2f V\n", data.pack_voltage);
}

// Extract specific field
float avg_cell_v = parser.extractField(msg, "avg_cell_voltage_mv");
if (!isnan(avg_cell_v)) {
    Serial.printf("Avg cell voltage: %.1f mV\n", avg_cell_v);
}
```

### Configuring Battery Protocol via Settings

```cpp
// In settings
BatteryConfig config = settings.batteries[0];

// Use built-in protocol
config.protocol_source = ProtocolSource::BUILTIN_DPOWER_48V;

// Or use custom protocol
config.protocol_source = ProtocolSource::CUSTOM_PROTOCOL;
strlcpy(config.protocol_path, "/protocols/custom_0.json",
        sizeof(config.protocol_path));

settings.updateBatteryConfig(0, config);
settings.save();
```

## Integration Points

### 1. Main Application Setup

```cpp
void setup() {
    // Initialize protocol loader
    Protocol::Loader protocolLoader;
    protocolLoader.begin();

    // Initialize web server with protocol loader
    webServer.begin(&settingsManager, &batteryManager,
                    &canLogger, &protocolLoader);

    // Initialize battery manager with protocols
    batteryManager.begin();
}
```

### 2. Battery Manager Integration

The battery manager should load the appropriate protocol for each battery based on the configuration:

```cpp
void BatteryManager::begin() {
    for (uint8_t i = 0; i < settings.num_batteries; i++) {
        if (!settings.batteries[i].enabled) continue;

        const Protocol::Definition* proto = nullptr;

        // Load protocol based on source
        if (settings.batteries[i].protocol_source ==
            ProtocolSource::BUILTIN_DPOWER_48V) {
            proto = Protocol::getBuiltinProtocol(
                Protocol::BuiltinId::DPOWER_48V_13S);
        }
        else if (settings.batteries[i].protocol_source ==
                 ProtocolSource::CUSTOM_PROTOCOL) {
            // Load from SPIFFS
            static Protocol::Definition custom_protos[MAX_BATTERY_MODULES];
            if (protocolLoader.loadFromFile(
                settings.batteries[i].protocol_path,
                custom_protos[i])) {
                proto = &custom_protos[i];
            }
        }

        // Set protocol for this battery's parser
        if (proto) {
            batteries[i].parser.setProtocol(proto);
        }
    }
}
```

## D-Power 48V 13S Protocol

Based on the CANBUS analyzer findings:

### Message 0x202 - Total Pack Voltage
- **Byte 0-1 (LE)**: Total voltage in mV
- **Calculated**: Average cell voltage = total / 13 cells
- **Period**: 100ms

### Message 0x203 - Cell Data
- **Byte 0**: Cell index (increments sequentially)
- **Byte 2-3 (LE)**: Cell voltage 1 (mV)
- **Byte 4-5 (LE)**: Cell voltage 2 (mV)
- **Byte 6-7 (LE)**: Cell voltage 3 (mV)
- **Period**: 50ms

### Message 0x204 - State/Command
- **Byte 0**: State value
  - 0x22 (34): Charging phase 1
  - 0x21 (33): Charging phase 2
  - 0x20 (32): Charging phase 3
  - 0x10 (16): Charge complete
  - 0x00 (0): Idle
- **Period**: 100ms

## Testing

### Upload Test Protocol

```bash
curl -X POST http://device-ip/api/protocols/upload \
  -H "Content-Type: application/json" \
  -d @data/protocols/dpower_48v_13s.json
```

### Fetch from URL

```bash
curl -X POST http://device-ip/api/protocols/fetch \
  -H "Content-Type: application/json" \
  -d '{"url":"https://example.com/protocol.json"}'
```

### List Protocols

```bash
curl http://device-ip/api/protocols
```

### Delete Protocol

```bash
curl -X DELETE http://device-ip/api/protocols/custom_0.json
```

## Web Interface Integration

Add to the battery configuration page:

```html
<div class="protocol-selector">
  <label>Protocol:</label>
  <select id="protocol-select">
    <option value="builtin_0">D-power 48V 13S (built-in)</option>
    <option value="builtin_1">Generic BMS (built-in)</option>
    <!-- Custom protocols loaded dynamically -->
  </select>

  <button onclick="uploadProtocol()">Upload Custom Protocol</button>
  <button onclick="fetchProtocol()">Fetch from URL</button>
</div>

<script>
// Load available protocols
fetch('/api/protocols')
  .then(r => r.json())
  .then(data => {
    const select = document.getElementById('protocol-select');
    data.custom.forEach(proto => {
      const opt = document.createElement('option');
      opt.value = 'custom_' + proto.id;
      opt.textContent = `${proto.name} (custom)`;
      select.appendChild(opt);
    });
  });
</script>
```

## Future Enhancements

1. **Protocol Repository**: GitHub-hosted protocol library
2. **Auto-Detection**: Analyze CAN traffic to suggest protocol matches
3. **Protocol Editor**: Web-based GUI for creating protocols
4. **Validation**: More extensive protocol validation with test data
5. **Export**: Export current protocol to JSON for sharing

## Troubleshooting

### Protocol Not Loading

Check serial output for errors:
```
SettingsManager: Protocol: D-power 48V 13S (built-in)
CANParser: Protocol set to 'Tianjin D-power 48V 13S'
```

### Invalid Custom Protocol

Use the validate endpoint:
```bash
curl http://device-ip/api/protocols/custom_0.json/validate
```

### SPIFFS Full

Check SPIFFS usage:
```cpp
Serial.printf("SPIFFS: %d / %d bytes used\n",
              SPIFFS.usedBytes(), SPIFFS.totalBytes());
```

Limit: ~10 custom protocols (2-5KB each)

## Migration Notes

For existing installations:

1. Existing batteries default to `BUILTIN_DPOWER_48V`
2. Settings are backwards compatible
3. Legacy `can_base_id` still supported
4. Old hardcoded parsers available as fallback

## References

- Protocol JSON Schema: `/data/protocols/SCHEMA.md`
- Example D-power Protocol: `/data/protocols/dpower_48v_13s.json`
- CAN Analyzer Tool: `/canbus_analyzer.tsx`
- Main Documentation: `/CLAUDE.md`
