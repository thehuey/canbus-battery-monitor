#include "settings.h"
#include <Preferences.h>

// NVS instance
static Preferences preferences;

SettingsManager::SettingsManager() {
    setDefaults();
}

bool SettingsManager::begin() {
    Serial.println("SettingsManager: Initializing...");

    // Try to load settings from NVS
    if (load()) {
        Serial.println("SettingsManager: Loaded settings from NVS");

        // Validate loaded settings
        if (validateSettings()) {
            Serial.println("SettingsManager: Settings validated successfully");
            return true;
        } else {
            Serial.println("SettingsManager: Validation failed, resetting to defaults");
            resetToDefaults();
            save();
            return false;
        }
    } else {
        Serial.println("SettingsManager: No valid settings found, using defaults");
        resetToDefaults();
        save();
        return false;
    }
}

bool SettingsManager::load() {
    if (!preferences.begin(NVS_NAMESPACE, true)) {  // true = read-only
        Serial.println("SettingsManager: Failed to open NVS namespace");
        return false;
    }

    // Check magic number first
    uint32_t magic = preferences.getUInt("magic", 0);
    if (magic != SETTINGS_MAGIC) {
        Serial.printf("SettingsManager: Invalid magic (0x%08X), expected 0x%08X\n",
                     magic, SETTINGS_MAGIC);
        preferences.end();
        return false;
    }

    // Load network settings
    preferences.getString("wifi_ssid", settings.wifi_ssid, sizeof(settings.wifi_ssid));
    preferences.getString("wifi_pass", settings.wifi_password, sizeof(settings.wifi_password));
    preferences.getString("mqtt_broker", settings.mqtt_broker, sizeof(settings.mqtt_broker));
    settings.mqtt_port = preferences.getUShort("mqtt_port", MQTT_DEFAULT_PORT);
    preferences.getString("mqtt_topic", settings.mqtt_topic_prefix, sizeof(settings.mqtt_topic_prefix));
    preferences.getString("mqtt_user", settings.mqtt_username, sizeof(settings.mqtt_username));
    preferences.getString("mqtt_pwd", settings.mqtt_password, sizeof(settings.mqtt_password));

    // Load CAN configuration
    settings.can_bitrate = preferences.getUInt("can_bitrate", CAN_BITRATE);

    // Load timing configuration
    settings.publish_interval_ms = preferences.getUShort("pub_interval", DEFAULT_PUBLISH_INTERVAL_MS);
    settings.sample_interval_ms = preferences.getUShort("sample_interval", DEFAULT_SAMPLE_INTERVAL_MS);
    settings.web_refresh_ms = preferences.getUShort("web_refresh", DEFAULT_WEB_REFRESH_MS);

    // Load battery configuration
    settings.num_batteries = preferences.getUChar("num_batteries", 1);

    // Clamp to valid range
    if (settings.num_batteries < 1 || settings.num_batteries > MAX_BATTERY_MODULES) {
        settings.num_batteries = 1;
    }

    // Load individual battery configs
    for (uint8_t i = 0; i < MAX_BATTERY_MODULES; i++) {
        char key[32];

        snprintf(key, sizeof(key), "bat%d_en", i);
        settings.batteries[i].enabled = preferences.getBool(key, i == 0);  // Battery 0 enabled by default

        snprintf(key, sizeof(key), "bat%d_name", i);
        char default_name[16];
        snprintf(default_name, sizeof(default_name), "Battery %d", i + 1);
        preferences.getString(key, settings.batteries[i].name, sizeof(settings.batteries[i].name));
        if (strlen(settings.batteries[i].name) == 0) {
            strlcpy(settings.batteries[i].name, default_name, sizeof(settings.batteries[i].name));
        }

        snprintf(key, sizeof(key), "bat%d_cur_off", i);
        settings.batteries[i].current_cal_offset = preferences.getFloat(key, ACS712_ZERO_CURRENT_MV);

        snprintf(key, sizeof(key), "bat%d_cur_scl", i);
        settings.batteries[i].current_cal_scale = preferences.getFloat(key, ACS712_20A_SENSITIVITY);

        snprintf(key, sizeof(key), "bat%d_vol_scl", i);
        settings.batteries[i].voltage_cal_scale = preferences.getFloat(key, VOLTAGE_DIVIDER_RATIO);

        snprintf(key, sizeof(key), "bat%d_can_id", i);
        settings.batteries[i].can_base_id = preferences.getUInt(key, 0);
    }

    settings.magic = magic;
    preferences.end();

    return true;
}

bool SettingsManager::save() {
    if (!preferences.begin(NVS_NAMESPACE, false)) {  // false = read-write
        Serial.println("SettingsManager: Failed to open NVS namespace for writing");
        return false;
    }

    Serial.println("SettingsManager: Saving settings to NVS...");

    // Save magic number
    preferences.putUInt("magic", SETTINGS_MAGIC);

    // Save network settings
    preferences.putString("wifi_ssid", settings.wifi_ssid);
    preferences.putString("wifi_pass", settings.wifi_password);
    preferences.putString("mqtt_broker", settings.mqtt_broker);
    preferences.putUShort("mqtt_port", settings.mqtt_port);
    preferences.putString("mqtt_topic", settings.mqtt_topic_prefix);
    preferences.putString("mqtt_user", settings.mqtt_username);
    preferences.putString("mqtt_pwd", settings.mqtt_password);

    // Save CAN configuration
    preferences.putUInt("can_bitrate", settings.can_bitrate);

    // Save timing configuration
    preferences.putUShort("pub_interval", settings.publish_interval_ms);
    preferences.putUShort("sample_interval", settings.sample_interval_ms);
    preferences.putUShort("web_refresh", settings.web_refresh_ms);

    // Save battery configuration
    preferences.putUChar("num_batteries", settings.num_batteries);

    // Save individual battery configs
    for (uint8_t i = 0; i < MAX_BATTERY_MODULES; i++) {
        char key[32];

        snprintf(key, sizeof(key), "bat%d_en", i);
        preferences.putBool(key, settings.batteries[i].enabled);

        snprintf(key, sizeof(key), "bat%d_name", i);
        preferences.putString(key, settings.batteries[i].name);

        snprintf(key, sizeof(key), "bat%d_cur_off", i);
        preferences.putFloat(key, settings.batteries[i].current_cal_offset);

        snprintf(key, sizeof(key), "bat%d_cur_scl", i);
        preferences.putFloat(key, settings.batteries[i].current_cal_scale);

        snprintf(key, sizeof(key), "bat%d_vol_scl", i);
        preferences.putFloat(key, settings.batteries[i].voltage_cal_scale);

        snprintf(key, sizeof(key), "bat%d_can_id", i);
        preferences.putUInt(key, settings.batteries[i].can_base_id);
    }

    preferences.end();

    Serial.println("SettingsManager: Settings saved successfully");
    return true;
}

void SettingsManager::resetToDefaults() {
    Serial.println("SettingsManager: Resetting to factory defaults");
    setDefaults();
}

void SettingsManager::setDefaults() {
    // Clear the structure
    memset(&settings, 0, sizeof(Settings));

    // Network defaults
    strlcpy(settings.wifi_ssid, "", sizeof(settings.wifi_ssid));
    strlcpy(settings.wifi_password, "", sizeof(settings.wifi_password));
    strlcpy(settings.mqtt_broker, "", sizeof(settings.mqtt_broker));
    settings.mqtt_port = MQTT_DEFAULT_PORT;
    strlcpy(settings.mqtt_topic_prefix, MQTT_TOPIC_PREFIX, sizeof(settings.mqtt_topic_prefix));
    strlcpy(settings.mqtt_username, "", sizeof(settings.mqtt_username));
    strlcpy(settings.mqtt_password, "", sizeof(settings.mqtt_password));

    // CAN defaults
    settings.can_bitrate = CAN_BITRATE;

    // Timing defaults
    settings.publish_interval_ms = DEFAULT_PUBLISH_INTERVAL_MS;
    settings.sample_interval_ms = DEFAULT_SAMPLE_INTERVAL_MS;
    settings.web_refresh_ms = DEFAULT_WEB_REFRESH_MS;

    // Battery defaults
    settings.num_batteries = 1;  // Start with 1 battery by default

    for (uint8_t i = 0; i < MAX_BATTERY_MODULES; i++) {
        settings.batteries[i].enabled = (i == 0);  // Only battery 0 enabled by default
        snprintf(settings.batteries[i].name, sizeof(settings.batteries[i].name),
                "Battery %d", i + 1);
        settings.batteries[i].current_cal_offset = ACS712_ZERO_CURRENT_MV;
        settings.batteries[i].current_cal_scale = ACS712_20A_SENSITIVITY;
        settings.batteries[i].voltage_cal_scale = VOLTAGE_DIVIDER_RATIO;
        settings.batteries[i].can_base_id = 0;  // Auto-detect
    }

    // Set magic number
    settings.magic = SETTINGS_MAGIC;
}

bool SettingsManager::validateSettings() {
    // Check magic number
    if (settings.magic != SETTINGS_MAGIC) {
        Serial.println("SettingsManager: Invalid magic number");
        return false;
    }

    // Validate CAN bitrate
    if (settings.can_bitrate != CAN_BITRATE) {
        Serial.printf("SettingsManager: Invalid CAN bitrate: %d\n", settings.can_bitrate);
        settings.can_bitrate = CAN_BITRATE;
    }

    // Validate timing intervals
    if (settings.sample_interval_ms < 10 || settings.sample_interval_ms > 10000) {
        Serial.printf("SettingsManager: Invalid sample interval: %d\n", settings.sample_interval_ms);
        settings.sample_interval_ms = DEFAULT_SAMPLE_INTERVAL_MS;
    }

    if (settings.publish_interval_ms < 100 || settings.publish_interval_ms > 60000) {
        Serial.printf("SettingsManager: Invalid publish interval: %d\n", settings.publish_interval_ms);
        settings.publish_interval_ms = DEFAULT_PUBLISH_INTERVAL_MS;
    }

    if (settings.web_refresh_ms < 100 || settings.web_refresh_ms > 10000) {
        Serial.printf("SettingsManager: Invalid web refresh: %d\n", settings.web_refresh_ms);
        settings.web_refresh_ms = DEFAULT_WEB_REFRESH_MS;
    }

    // Validate number of batteries
    if (settings.num_batteries < 1 || settings.num_batteries > MAX_BATTERY_MODULES) {
        Serial.printf("SettingsManager: Invalid battery count: %d\n", settings.num_batteries);
        settings.num_batteries = 1;
    }

    // Validate MQTT port
    if (settings.mqtt_port == 0) {
        settings.mqtt_port = MQTT_DEFAULT_PORT;
    }

    // Validate battery configs
    for (uint8_t i = 0; i < MAX_BATTERY_MODULES; i++) {
        // Validate name is null-terminated
        settings.batteries[i].name[sizeof(settings.batteries[i].name) - 1] = '\0';

        // Validate calibration values are reasonable
        if (settings.batteries[i].current_cal_offset < 0 ||
            settings.batteries[i].current_cal_offset > 5000) {
            settings.batteries[i].current_cal_offset = ACS712_ZERO_CURRENT_MV;
        }

        if (settings.batteries[i].current_cal_scale < 10 ||
            settings.batteries[i].current_cal_scale > 500) {
            settings.batteries[i].current_cal_scale = ACS712_20A_SENSITIVITY;
        }

        if (settings.batteries[i].voltage_cal_scale < 1 ||
            settings.batteries[i].voltage_cal_scale > 100) {
            settings.batteries[i].voltage_cal_scale = VOLTAGE_DIVIDER_RATIO;
        }
    }

    return true;
}

bool SettingsManager::updateBatteryConfig(uint8_t index, const BatteryConfig& config) {
    if (index >= MAX_BATTERY_MODULES) {
        Serial.printf("SettingsManager: Invalid battery index: %d\n", index);
        return false;
    }

    // Update the battery config
    settings.batteries[index] = config;

    // Ensure name is null-terminated
    settings.batteries[index].name[sizeof(settings.batteries[index].name) - 1] = '\0';

    Serial.printf("SettingsManager: Updated battery %d config\n", index);

    return true;
}

void SettingsManager::printSettings() const {
    Serial.println("\n========== Current Settings ==========");

    Serial.println("\n[Network]");
    Serial.printf("  WiFi SSID: %s\n", settings.wifi_ssid);
    Serial.printf("  WiFi Password: %s\n", strlen(settings.wifi_password) > 0 ? "***" : "(empty)");
    Serial.printf("  MQTT Broker: %s:%d\n", settings.mqtt_broker, settings.mqtt_port);
    Serial.printf("  MQTT Topic Prefix: %s\n", settings.mqtt_topic_prefix);
    Serial.printf("  MQTT Username: %s\n", settings.mqtt_username);
    Serial.printf("  MQTT Password: %s\n", strlen(settings.mqtt_password) > 0 ? "***" : "(empty)");

    Serial.println("\n[CAN Bus]");
    Serial.printf("  Bitrate: %d bps\n", settings.can_bitrate);

    Serial.println("\n[Timing]");
    Serial.printf("  Sample Interval: %d ms\n", settings.sample_interval_ms);
    Serial.printf("  Publish Interval: %d ms\n", settings.publish_interval_ms);
    Serial.printf("  Web Refresh: %d ms\n", settings.web_refresh_ms);

    Serial.println("\n[Batteries]");
    Serial.printf("  Active Count: %d\n", settings.num_batteries);

    for (uint8_t i = 0; i < settings.num_batteries; i++) {
        Serial.printf("\n  Battery %d:\n", i);
        Serial.printf("    Enabled: %s\n", settings.batteries[i].enabled ? "Yes" : "No");
        Serial.printf("    Name: %s\n", settings.batteries[i].name);
        Serial.printf("    Current Cal Offset: %.2f mV\n", settings.batteries[i].current_cal_offset);
        Serial.printf("    Current Cal Scale: %.2f mV/A\n", settings.batteries[i].current_cal_scale);
        Serial.printf("    Voltage Cal Scale: %.2f\n", settings.batteries[i].voltage_cal_scale);
        Serial.printf("    CAN Base ID: 0x%03X %s\n",
                     settings.batteries[i].can_base_id,
                     settings.batteries[i].can_base_id == 0 ? "(auto)" : "");
    }

    Serial.println("\n======================================\n");
}

bool SettingsManager::clearNVS() {
    Serial.println("SettingsManager: Clearing all NVS data...");

    if (!preferences.begin(NVS_NAMESPACE, false)) {
        Serial.println("SettingsManager: Failed to open NVS namespace");
        return false;
    }

    bool result = preferences.clear();
    preferences.end();

    if (result) {
        Serial.println("SettingsManager: NVS cleared successfully");
        setDefaults();  // Reset to defaults in memory
    } else {
        Serial.println("SettingsManager: Failed to clear NVS");
    }

    return result;
}
