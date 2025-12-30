#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include "config.h"

// Battery-specific configuration
struct BatteryConfig {
    bool enabled;
    char name[16];                  // e.g., "Battery 1", "Front", "Rear"
    float current_cal_offset;       // Zero-current offset (mV)
    float current_cal_scale;        // mV per Amp
    float voltage_cal_scale;        // Voltage divider ratio
    uint32_t can_base_id;           // Base CAN ID for this battery (0 = auto)
};

// Global system settings
struct Settings {
    // Network Configuration
    char wifi_ssid[32];
    char wifi_password[64];
    char mqtt_broker[64];
    uint16_t mqtt_port;
    char mqtt_topic_prefix[32];
    char mqtt_username[32];
    char mqtt_password[64];

    // CAN Configuration
    uint32_t can_bitrate;           // Fixed at 500000

    // Timing Configuration
    uint16_t publish_interval_ms;   // MQTT publish rate (default: 1000)
    uint16_t sample_interval_ms;    // ADC sample rate (default: 100)
    uint16_t web_refresh_ms;        // WebSocket push rate (default: 500)

    // Battery Modules (modular: 1-5)
    uint8_t num_batteries;          // Active battery count (1-5)
    BatteryConfig batteries[MAX_BATTERY_MODULES];

    // Magic number for validation
    uint32_t magic;                 // Used to detect valid configuration
};

// Settings management class
class SettingsManager {
public:
    SettingsManager();

    // Initialize and load from NVS
    bool begin();

    // Load/save settings
    bool load();
    bool save();

    // Reset to factory defaults
    void resetToDefaults();

    // Getters
    Settings& getSettings() { return settings; }
    const Settings& getSettings() const { return settings; }

    // Update specific battery config
    bool updateBatteryConfig(uint8_t index, const BatteryConfig& config);

    // Utility functions
    void printSettings() const;  // Debug output of current settings
    bool clearNVS();             // Erase all NVS data

private:
    Settings settings;

    // Set default values
    void setDefaults();

    // Validation
    bool validateSettings();

    static constexpr uint32_t SETTINGS_MAGIC = 0xEB10E001;  // Magic number for eBike
};

#endif // SETTINGS_H
