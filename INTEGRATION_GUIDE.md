# Protocol System Integration Guide

This guide shows how to integrate the new protocol system into your existing eBike Battery Monitor codebase.

## Required Changes to Existing Files

### 1. Update `src/network/web_server.cpp`

Add protocol loader initialization in the `begin()` method:

```cpp
bool WebServer::begin(SettingsManager* settings, BatteryManager* batteries,
                      CANLogger* canLog, Protocol::Loader* protocolLoader) {
    settings_ = settings;
    batteries_ = batteries;
    can_logger_ = canLog;
    protocol_loader_ = protocolLoader;  // Add this line

    // Rest of begin() implementation...
}
```

Add protocol routes in `setupAPIEndpoints()`:

```cpp
void WebServer::setupAPIEndpoints() {
    // Existing routes...

    // Protocol management endpoints
    server_.on("/api/protocols", HTTP_GET,
        [this](AsyncWebServerRequest* request) {
            handleGetProtocols(request);
        });

    server_.on("/api/protocols/builtin", HTTP_GET,
        [this](AsyncWebServerRequest* request) {
            handleGetBuiltinProtocols(request);
        });

    server_.on("/api/protocols/custom", HTTP_GET,
        [this](AsyncWebServerRequest* request) {
            handleGetCustomProtocols(request);
        });

    // Get specific protocol
    server_.on("^\\/api\\/protocols\\/(.+)$", HTTP_GET,
        [this](AsyncWebServerRequest* request) {
            String id = request->pathArg(0);
            handleGetProtocol(request, id);
        });

    // Upload protocol
    server_.on("/api/protocols/upload", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            // Handled in onBody
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
               size_t index, size_t total) {
            if (index == 0) {
                // First chunk
                handleUploadProtocol(request, data, len);
            }
        });

    // Fetch protocol from URL
    server_.on("/api/protocols/fetch", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            // Handled in onBody
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
               size_t index, size_t total) {
            if (index == 0) {
                handleFetchProtocol(request, data, len);
            }
        });

    // Delete protocol
    server_.on("^\\/api\\/protocols\\/(.+)$", HTTP_DELETE,
        [this](AsyncWebServerRequest* request) {
            String id = request->pathArg(0);
            handleDeleteProtocol(request, id);
        });

    // Validate protocol
    server_.on("^\\/api\\/protocols\\/(.+)\\/validate$", HTTP_GET,
        [this](AsyncWebServerRequest* request) {
            String id = request->pathArg(0);
            handleValidateProtocol(request, id);
        });
}
```

### 2. Update `src/battery/battery_manager.h`

Add protocol loader member:

```cpp
#include "../can/protocol_loader.h"

class BatteryManager {
public:
    // Add protocol loader parameter
    void begin(Protocol::Loader* protocolLoader = nullptr);

private:
    Protocol::Loader* protocol_loader_;

    // Storage for custom protocols (persistent)
    static Protocol::Definition custom_protocols_[MAX_BATTERY_MODULES];
};
```

### 3. Update `src/battery/battery_manager.cpp`

Initialize protocols for each battery:

```cpp
// Static storage for custom protocols
Protocol::Definition BatteryManager::custom_protocols_[MAX_BATTERY_MODULES];

void BatteryManager::begin(Protocol::Loader* protocolLoader) {
    protocol_loader_ = protocolLoader;

    Serial.println("BatteryManager: Initializing batteries...");

    for (uint8_t i = 0; i < settings_->num_batteries; i++) {
        if (!settings_->batteries[i].enabled) {
            Serial.printf("Battery %d: Disabled\n", i);
            continue;
        }

        Serial.printf("Battery %d: Initializing '%s'\n",
                     i, settings_->batteries[i].name);

        // Load and set protocol
        const Protocol::Definition* proto = nullptr;

        switch (settings_->batteries[i].protocol_source) {
            case ProtocolSource::BUILTIN_DPOWER_48V:
                proto = Protocol::getBuiltinProtocol(
                    Protocol::BuiltinId::DPOWER_48V_13S);
                Serial.printf("  Using built-in D-power 48V protocol\n");
                break;

            case ProtocolSource::BUILTIN_GENERIC_BMS:
                proto = Protocol::getBuiltinProtocol(
                    Protocol::BuiltinId::GENERIC_BMS);
                Serial.printf("  Using built-in Generic BMS protocol\n");
                break;

            case ProtocolSource::CUSTOM_PROTOCOL:
                if (protocol_loader_) {
                    if (protocol_loader_->loadFromFile(
                        settings_->batteries[i].protocol_path,
                        custom_protocols_[i])) {
                        proto = &custom_protocols_[i];
                        Serial.printf("  Loaded custom protocol: %s\n",
                                    custom_protocols_[i].name);
                    } else {
                        Serial.printf("  Failed to load custom protocol: %s\n",
                                    protocol_loader_->getLastError());
                    }
                }
                break;
        }

        // Set protocol for this battery's parser
        if (proto) {
            batteries_[i].parser.setProtocol(proto);
        } else {
            Serial.printf("  Warning: No protocol set for battery %d\n", i);
        }

        // Initialize battery module
        batteries_[i].begin(i, &settings_->batteries[i]);
    }
}
```

### 4. Update `src/battery/battery_module.h`

Add CAN parser member:

```cpp
#include "../can/can_parser.h"

class BatteryModule {
public:
    // ... existing members ...

    CANParser parser;  // Add this

    // ... existing methods ...
};
```

### 5. Update `src/battery/battery_module.cpp`

Use parser to decode CAN messages:

```cpp
void BatteryModule::processCANMessage(const CANMessage& msg) {
    CANBatteryData data;

    if (parser.parseMessage(msg, data)) {
        // Update module data from parsed CAN message
        if (data.valid) {
            pack_voltage_ = data.pack_voltage;
            pack_current_ = data.pack_current;
            soc_ = data.soc;
            temp1_ = data.temp1;
            temp2_ = data.temp2;
            status_flags_ = data.status_flags;
            last_can_update_ = millis();
        }
    }

    // You can also extract specific fields
    float avg_cell_v = parser.extractField(msg, "avg_cell_voltage_mv");
    if (!isnan(avg_cell_v)) {
        avg_cell_voltage_ = avg_cell_v;
    }
}
```

### 6. Update `src/main.cpp`

Initialize protocol loader and pass to components:

```cpp
#include "can/protocol_loader.h"

// Global instances
SettingsManager settingsManager;
BatteryManager batteryManager;
CANLogger canLogger;
Protocol::Loader protocolLoader;  // Add this

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=== eBike Battery CANBUS Monitor ===\n");

    // Initialize settings
    settingsManager.begin();
    settingsManager.printSettings();

    // Initialize protocol loader
    if (!protocolLoader.begin()) {
        Serial.println("ERROR: Failed to initialize protocol loader");
    }

    // Initialize CAN bus
    canDriver.begin();

    // Initialize battery manager with protocol loader
    batteryManager.begin(&protocolLoader);  // Pass protocol loader

    // Initialize CAN logger
    canLogger.begin();

    // Initialize WiFi
    wifiManager.begin(&settingsManager);

    // Initialize web server with protocol loader
    webServer.begin(&settingsManager, &batteryManager, &canLogger,
                    &protocolLoader);  // Pass protocol loader

    Serial.println("\nSetup complete!\n");
}
```

### 7. Update `platformio.ini`

Ensure proper build flags and file inclusion:

```ini
[env:nodemcu-32s]
; ... existing settings ...

build_flags =
    ${env:nodemcu-32s.build_flags}
    -DPROTOCOL_SYSTEM_ENABLED

; Ensure protocol files are included
build_src_filter =
    +<*>
    +<can/protocol.cpp>
    +<can/builtin_protocols.cpp>
    +<can/protocol_loader.cpp>
    +<network/protocol_api.cpp>
```

## Compilation Notes

### Include Order

Make sure to include headers in this order to avoid dependency issues:

```cpp
#include "can/protocol.h"           // Base protocol structures
#include "can/builtin_protocols.h"  // Built-in protocol definitions
#include "can/protocol_loader.h"    // JSON protocol loader
#include "can/can_parser.h"         // Protocol-aware parser
```

### Build Order

The build system should compile files in this order:
1. `protocol.cpp` - Core data structures and helpers
2. `builtin_protocols.cpp` - Built-in protocol definitions
3. `protocol_loader.cpp` - JSON loader
4. `can_parser.cpp` - Parser using protocols
5. `protocol_api.cpp` - Web API handlers

## Testing Checklist

### 1. Built-in Protocol Test

```cpp
// In setup() or a test function
const Protocol::Definition* proto = Protocol::getBuiltinProtocol(
    Protocol::BuiltinId::DPOWER_48V_13S);

if (proto) {
    Serial.printf("Protocol loaded: %s\n", proto->name);
    Serial.printf("Messages: %d\n", proto->message_count);

    // Test finding a message
    const Protocol::Message* msg = proto->findMessage(0x202);
    if (msg) {
        Serial.printf("Found message 0x202: %s\n", msg->name);
        Serial.printf("Fields: %d\n", msg->field_count);
    }
} else {
    Serial.println("ERROR: Failed to load built-in protocol");
}
```

### 2. Protocol Loader Test

```cpp
// Test loading from file
Protocol::Definition custom;
if (protocolLoader.loadFromFile("/protocols/custom_0.json", custom)) {
    Serial.printf("Custom protocol loaded: %s\n", custom.name);

    if (protocolLoader.validate(custom)) {
        Serial.println("Protocol is valid");
    } else {
        Serial.println("Protocol validation failed");
    }
} else {
    Serial.printf("Load failed: %s\n", protocolLoader.getLastError());
}
```

### 3. CAN Parser Test

```cpp
// Create a test CAN message
CANMessage test_msg;
test_msg.id = 0x202;
test_msg.dlc = 8;
test_msg.data[0] = 0xCE;  // 53070 in LE = 53070 mV
test_msg.data[1] = 0xCF;
test_msg.data[2] = 0x00;
test_msg.data[3] = 0x00;
// ... rest of data ...

// Parse it
CANParser parser;
parser.setProtocol(proto);

CANBatteryData data;
if (parser.parseMessage(test_msg, data)) {
    Serial.printf("Pack voltage: %.2f V\n", data.pack_voltage);

    float avg = parser.extractField(test_msg, "avg_cell_voltage_mv");
    Serial.printf("Avg cell: %.1f mV\n", avg);
}
```

### 4. Web API Test

```bash
# Test from command line
curl http://192.168.1.100/api/protocols
curl http://192.168.1.100/api/protocols/builtin_0
```

## Memory Optimization

If you encounter memory issues:

### Reduce Protocol Limits

In `src/can/protocol.h`, adjust these:

```cpp
#define MAX_PROTOCOL_NAME_LEN 32       // Was 48
#define MAX_FIELDS_PER_MESSAGE 8       // Was 16
#define MAX_MESSAGES_PER_PROTOCOL 8    // Was 16
#define MAX_ENUM_VALUES 8              // Was 16
```

### Limit Custom Protocols

```cpp
// In battery_manager.cpp, reduce array size
static Protocol::Definition custom_protocols_[2];  // Only 2 custom protocols
```

### Use PROGMEM for Built-in Protocols

```cpp
// In builtin_protocols.cpp
static const Field PROGMEM dpower_0x202_fields[] = {
    // ... fields ...
};
```

## Troubleshooting

### Linker Errors

If you get undefined reference errors:

1. Make sure `protocol_api.cpp` is included in the build
2. Check that all source files are in `build_src_filter`
3. Verify include paths are correct

### SPIFFS Mount Failures

```cpp
// In protocolLoader.begin()
if (!SPIFFS.begin(true)) {  // true = format on fail
    Serial.println("SPIFFS mount failed, formatting...");
    SPIFFS.format();
    if (!SPIFFS.begin()) {
        return false;
    }
}
```

### Protocol Parse Errors

Enable debug output in `protocol_loader.cpp`:

```cpp
#define PROTOCOL_DEBUG 1

#if PROTOCOL_DEBUG
  Serial.printf("Parsing field: %s\n", field.name);
  Serial.printf("  Offset: %d, Length: %d\n", field.byte_offset, field.length);
#endif
```

## Next Steps

1. Test with your actual battery hardware
2. Adjust D-power protocol based on real CAN data
3. Create additional protocol definitions
4. Build web UI for protocol management
5. Document your battery's specific protocol

## Support

For issues or questions:
- Check `/workspace/PROTOCOL_SYSTEM.md` for detailed documentation
- Review `/data/protocols/SCHEMA.md` for JSON schema
- Use the CAN analyzer tool to reverse-engineer protocols
- Monitor serial output for debug messages
