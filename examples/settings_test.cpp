/*
 * Settings Manager Test Example
 *
 * This example demonstrates how to use the SettingsManager class
 * to load, modify, and save settings to NVS.
 *
 * Upload this to your ESP32 to test the settings manager functionality.
 */

#include <Arduino.h>
#include "../src/config/config.h"
#include "../src/config/settings.h"

SettingsManager settingsManager;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n========================================");
    Serial.println("Settings Manager Test");
    Serial.println("========================================\n");

    // Test 1: Initialize settings manager
    Serial.println("Test 1: Initializing SettingsManager...");
    if (settingsManager.begin()) {
        Serial.println("✓ Settings loaded from NVS");
    } else {
        Serial.println("✓ Using default settings (first boot)");
    }

    // Test 2: Print current settings
    Serial.println("\nTest 2: Print current settings");
    settingsManager.printSettings();

    // Test 3: Modify settings
    Serial.println("Test 3: Modifying settings...");
    Settings& settings = settingsManager.getSettings();

    // Update WiFi credentials
    strlcpy(settings.wifi_ssid, "MyHomeWiFi", sizeof(settings.wifi_ssid));
    strlcpy(settings.wifi_password, "MySecretPassword", sizeof(settings.wifi_password));

    // Update MQTT broker
    strlcpy(settings.mqtt_broker, "192.168.1.100", sizeof(settings.mqtt_broker));
    settings.mqtt_port = 1883;

    // Update battery configuration
    settings.num_batteries = 2;
    settings.batteries[0].enabled = true;
    strlcpy(settings.batteries[0].name, "Front Battery", sizeof(settings.batteries[0].name));

    settings.batteries[1].enabled = true;
    strlcpy(settings.batteries[1].name, "Rear Battery", sizeof(settings.batteries[1].name));

    Serial.println("✓ Settings modified in memory");

    // Test 4: Save settings
    Serial.println("\nTest 4: Saving settings to NVS...");
    if (settingsManager.save()) {
        Serial.println("✓ Settings saved successfully");
    } else {
        Serial.println("✗ Failed to save settings");
    }

    // Test 5: Reload settings to verify
    Serial.println("\nTest 5: Reloading settings to verify...");
    if (settingsManager.load()) {
        Serial.println("✓ Settings reloaded successfully");
        settingsManager.printSettings();
    } else {
        Serial.println("✗ Failed to reload settings");
    }

    // Test 6: Update individual battery config
    Serial.println("Test 6: Update individual battery config...");
    BatteryConfig battConfig;
    battConfig.enabled = true;
    strlcpy(battConfig.name, "Test Battery", sizeof(battConfig.name));
    battConfig.current_cal_offset = 2510.0f;
    battConfig.current_cal_scale = 100.0f;
    battConfig.voltage_cal_scale = 21.5f;
    battConfig.can_base_id = 0x100;

    if (settingsManager.updateBatteryConfig(0, battConfig)) {
        Serial.println("✓ Battery config updated");
    } else {
        Serial.println("✗ Failed to update battery config");
    }

    // Test 7: Clear NVS (optional - uncomment to test)
    // Serial.println("\nTest 7: Clearing NVS...");
    // if (settingsManager.clearNVS()) {
    //     Serial.println("✓ NVS cleared");
    //     settingsManager.printSettings();
    // } else {
    //     Serial.println("✗ Failed to clear NVS");
    // }

    Serial.println("\n========================================");
    Serial.println("All tests completed!");
    Serial.println("========================================\n");
}

void loop() {
    // Nothing to do in loop
    delay(1000);
}
