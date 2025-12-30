# Configuration Module

This module handles persistent storage and management of system settings using ESP32's NVS (Non-Volatile Storage).

## Files

- `config.h` - Compile-time constants, pin definitions, and default values
- `settings.h` - Settings structures and SettingsManager class declaration
- `settings.cpp` - SettingsManager implementation with NVS integration

## Settings Structure

The system stores two main configuration structures:

### BatteryConfig
Per-battery configuration (up to 5 batteries):
```cpp
struct BatteryConfig {
    bool enabled;                   // Battery enabled/disabled
    char name[16];                  // Custom name (e.g., "Front", "Rear")
    float current_cal_offset;       // Zero-current offset in mV
    float current_cal_scale;        // Sensitivity in mV/A
    float voltage_cal_scale;        // Voltage divider ratio
    uint32_t can_base_id;          // Base CAN ID (0 = auto-detect)
};
```

### Settings
Global system settings:
```cpp
struct Settings {
    // Network
    char wifi_ssid[32];
    char wifi_password[64];
    char mqtt_broker[64];
    uint16_t mqtt_port;
    char mqtt_topic_prefix[32];
    char mqtt_username[32];
    char mqtt_password[64];

    // CAN Configuration
    uint32_t can_bitrate;           // Fixed at 500000 bps

    // Timing
    uint16_t publish_interval_ms;   // MQTT publish rate
    uint16_t sample_interval_ms;    // ADC sample rate
    uint16_t web_refresh_ms;        // WebSocket refresh rate

    // Battery Modules
    uint8_t num_batteries;          // Active battery count (1-5)
    BatteryConfig batteries[MAX_BATTERY_MODULES];

    uint32_t magic;                 // Validation magic number
};
```

## SettingsManager API

### Initialization

```cpp
SettingsManager settingsManager;

void setup() {
    if (!settingsManager.begin()) {
        // First boot or invalid settings - using defaults
    }
}
```

### Accessing Settings

```cpp
// Get read/write access
Settings& settings = settingsManager.getSettings();
settings.num_batteries = 2;

// Get read-only access
const Settings& settings = settingsManager.getSettings();
Serial.println(settings.wifi_ssid);
```

### Saving Settings

```cpp
Settings& settings = settingsManager.getSettings();

// Modify settings
strlcpy(settings.wifi_ssid, "MyWiFi", sizeof(settings.wifi_ssid));
settings.mqtt_port = 1883;

// Save to NVS
if (settingsManager.save()) {
    Serial.println("Settings saved!");
}
```

### Loading Settings

```cpp
// Reload from NVS (usually done automatically in begin())
if (settingsManager.load()) {
    Serial.println("Settings reloaded");
}
```

### Updating Battery Configuration

```cpp
BatteryConfig config;
config.enabled = true;
strlcpy(config.name, "Front Battery", sizeof(config.name));
config.current_cal_offset = 2500.0f;
config.current_cal_scale = 100.0f;  // 20A variant
config.voltage_cal_scale = 20.0f;
config.can_base_id = 0x100;

if (settingsManager.updateBatteryConfig(0, config)) {
    settingsManager.save();  // Don't forget to save!
}
```

### Resetting to Defaults

```cpp
// Reset to factory defaults
settingsManager.resetToDefaults();
settingsManager.save();
```

### Clearing NVS

```cpp
// Erase all stored settings
if (settingsManager.clearNVS()) {
    Serial.println("NVS cleared, using defaults");
}
```

### Printing Settings

```cpp
// Debug output of all settings
settingsManager.printSettings();
```

## NVS Key Mapping

Settings are stored in NVS namespace `ebike_config` with these keys:

| Key | Type | Description |
|-----|------|-------------|
| `magic` | uint32 | Validation magic number (0xEB1KE001) |
| `wifi_ssid` | string | WiFi SSID |
| `wifi_pass` | string | WiFi password |
| `mqtt_broker` | string | MQTT broker address |
| `mqtt_port` | uint16 | MQTT port |
| `mqtt_topic` | string | MQTT topic prefix |
| `mqtt_user` | string | MQTT username |
| `mqtt_pwd` | string | MQTT password |
| `can_bitrate` | uint32 | CAN bitrate (500000) |
| `pub_interval` | uint16 | MQTT publish interval (ms) |
| `sample_interval` | uint16 | Sensor sample interval (ms) |
| `web_refresh` | uint16 | WebSocket refresh interval (ms) |
| `num_batteries` | uint8 | Number of active batteries |
| `bat0_en` - `bat4_en` | bool | Battery enabled flags |
| `bat0_name` - `bat4_name` | string | Battery names |
| `bat0_cur_off` - `bat4_cur_off` | float | Current calibration offsets |
| `bat0_cur_scl` - `bat4_cur_scl` | float | Current calibration scales |
| `bat0_vol_scl` - `bat4_vol_scl` | float | Voltage calibration scales |
| `bat0_can_id` - `bat4_can_id` | uint32 | CAN base IDs |

## Default Values

Default values are defined in `config.h`:

```cpp
// Network
MQTT_DEFAULT_PORT = 1883
MQTT_TOPIC_PREFIX = "ebike"

// Timing
DEFAULT_SAMPLE_INTERVAL_MS = 100
DEFAULT_PUBLISH_INTERVAL_MS = 1000
DEFAULT_WEB_REFRESH_MS = 500

// CAN
CAN_BITRATE = 500000

// Sensors
ACS712_ZERO_CURRENT_MV = 2500.0
ACS712_20A_SENSITIVITY = 100.0  // mV/A
VOLTAGE_DIVIDER_RATIO = 20.0
```

## Validation

The SettingsManager validates settings on load:
- Magic number must match `0xEB1KE001`
- CAN bitrate must be 500000
- Timing intervals must be within reasonable ranges
- Battery count must be 1-5
- Calibration values must be within expected ranges
- Strings are null-terminated

Invalid settings are automatically corrected or reset to defaults.

## Example Usage

See `/examples/settings_test.cpp` for a complete working example.

## Thread Safety

⚠️ **Note**: The SettingsManager is not thread-safe. Access from multiple tasks should be protected with a mutex or semaphore.

## Memory Usage

The Settings structure uses approximately:
- Network config: ~200 bytes
- CAN/Timing: ~20 bytes
- Battery configs: ~180 bytes (36 bytes × 5 batteries)
- **Total: ~400 bytes**

NVS Flash usage depends on the number of stored keys, typically ~2-4 KB.
