# Build Optimizations Applied

## Summary

The firmware build was optimized to fit within available flash memory by applying several compiler optimizations and feature flags. The final build uses **1,317,701 bytes (41.9%)** of the 3MB flash partition.

## Optimizations Applied

### 1. Partition Scheme Change
- **Changed from**: `default.csv` (1.31MB app space)
- **Changed to**: `huge_app.csv` (3MB app space)
- **Location**: `platformio.ini` line 36
- **Impact**: Provides 3,145,728 bytes for application code

### 2. Compiler Optimizations
Added the following build flags to `platformio.ini`:

```ini
-Os                      # Optimize for size
-ffunction-sections      # Place each function in separate section
-fdata-sections          # Place each data item in separate section
-Wl,--gc-sections        # Remove unused sections during linking
-DNDEBUG                 # Disable assert() macros
```

**Impact**: Reduced code size by approximately 4-5KB

### 3. Debug Level Reduction
- **Changed from**: `CORE_DEBUG_LEVEL=3` (verbose debugging)
- **Changed to**: `CORE_DEBUG_LEVEL=0` (minimal logging)
- **Also disabled**: `CONFIG_ARDUHAL_LOG_COLORS=0`
- **Impact**: Significant reduction in debug strings and logging code

### 4. MQTT TLS Disabled
- **Flag**: `-DMQTT_DISABLE_TLS=1`
- **Impact**: Removed ~2KB root CA certificate and TLS-related code
- **Note**: MQTT will use insecure connections. For production use with HiveMQ Cloud, you should:
  - Remove this flag from `platformio.ini`
  - Accept the larger firmware size
  - Or use a custom partition scheme that balances app/SPIFFS sizes

**Code changes**:
- `src/network/mqtt_client.cpp`: Certificate and `setupTLS()` wrapped in `#ifndef MQTT_DISABLE_TLS`

### 5. Generic BMS Protocol Disabled
- **Flag**: `-DGENERIC_BMS_DISABLED=1`
- **Impact**: Removed Generic BMS protocol definition, saving ~1-2KB
- **Note**: Only D-power 48V 13S protocol is built-in. Generic BMS can still be loaded as a custom protocol from SPIFFS.

**Code changes**:
- `src/can/builtin_protocols.cpp`: `createGenericBMSProtocol()` and related code wrapped in `#ifndef GENERIC_BMS_DISABLED`

## Memory Usage (Final Build)

```
RAM:   [===       ]  27.4% (used 89,916 bytes from 327,680 bytes)
Flash: [====      ]  41.9% (used 1,317,701 bytes from 3,145,728 bytes)
```

## Re-enabling Disabled Features

### To Re-enable MQTT TLS:
1. Edit `platformio.ini`
2. Remove or comment out: `-DMQTT_DISABLE_TLS=1`
3. Rebuild the firmware
4. Note: This will add ~2KB to flash usage

### To Re-enable Generic BMS Protocol:
1. Edit `platformio.ini`
2. Remove or comment out: `-DGENERIC_BMS_DISABLED=1`
3. Rebuild the firmware
4. Note: This will add ~1-2KB to flash usage

### If Flash Space Becomes an Issue Again:
Consider these options:
1. Use a different partition scheme that better balances your needs (see `/home/node/.platformio/packages/framework-arduinoespressif32/tools/partitions/`)
2. Further reduce `CORE_DEBUG_LEVEL` or disable more debug output
3. Remove unused features or protocols
4. Optimize ArduinoJson buffer sizes if they're larger than needed

## Build Command

```bash
pio run
```

To see detailed memory usage:
```bash
pio run --verbose
```

## Partition Layout (huge_app.csv)

| Name     | Type     | SubType  | Offset   | Size     | Usage              |
|----------|----------|----------|----------|----------|--------------------|
| nvs      | data     | nvs      | 0x9000   | 0x5000   | Non-volatile storage (settings) |
| otadata  | data     | ota      | 0xe000   | 0x2000   | OTA data partition |
| app0     | app      | ota_0    | 0x10000  | 0x300000 | Application (3MB)  |
| spiffs   | data     | spiffs   | 0x310000 | 0xE0000  | File system (896KB)|
| coredump | data     | coredump | 0x3F0000 | 0x10000  | Core dump storage  |

## Notes

- The `huge_app.csv` partition is suitable for boards with 4MB flash or larger
- If your ESP32 has less than 4MB flash, you may need to use a different partition scheme
- Current configuration prioritizes application size over SPIFFS storage
- SPIFFS still has 896KB available for storing:
  - Custom protocol JSON files
  - CAN log files
  - Web interface static files
