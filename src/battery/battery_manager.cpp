#include "battery_manager.h"

BatteryManager::BatteryManager() : active_count(0) {
}

void BatteryManager::begin(uint8_t num_batteries) {
    if (num_batteries > MAX_BATTERY_MODULES) {
        Serial.printf("BatteryManager: Warning - requested %d batteries, limiting to %d\n",
                     num_batteries, MAX_BATTERY_MODULES);
        num_batteries = MAX_BATTERY_MODULES;
    }

    if (num_batteries < 1) {
        Serial.println("BatteryManager: Warning - at least 1 battery required, setting to 1");
        num_batteries = 1;
    }

    active_count = num_batteries;

    // Initialize each battery module
    for (uint8_t i = 0; i < active_count; i++) {
        char default_name[16];
        snprintf(default_name, sizeof(default_name), "Battery %d", i + 1);
        batteries[i].begin(i, default_name);
    }

    Serial.printf("BatteryManager: Initialized with %d battery module(s)\n", active_count);
}

void BatteryManager::update() {
    // This function can be used for periodic maintenance tasks
    // Currently, updates are handled via sensor readings and CAN messages
}

BatteryModule* BatteryManager::getBattery(uint8_t index) {
    if (!isValidIndex(index)) {
        return nullptr;
    }
    return &batteries[index];
}

const BatteryModule* BatteryManager::getBattery(uint8_t index) const {
    if (!isValidIndex(index)) {
        return nullptr;
    }
    return &batteries[index];
}

float BatteryManager::getTotalPower() const {
    float total = 0.0f;

    for (uint8_t i = 0; i < active_count; i++) {
        if (batteries[i].isEnabled() && batteries[i].isDataFresh()) {
            total += batteries[i].getPower();
        }
    }

    return total;
}

float BatteryManager::getTotalCurrent() const {
    float total = 0.0f;

    for (uint8_t i = 0; i < active_count; i++) {
        if (batteries[i].isEnabled() && batteries[i].isDataFresh()) {
            total += batteries[i].getCurrent();
        }
    }

    return total;
}

float BatteryManager::getAverageVoltage() const {
    float total = 0.0f;
    uint8_t count = 0;

    for (uint8_t i = 0; i < active_count; i++) {
        if (batteries[i].isEnabled() && batteries[i].isDataFresh()) {
            total += batteries[i].getVoltage();
            count++;
        }
    }

    return (count > 0) ? (total / count) : 0.0f;
}

void BatteryManager::enableBattery(uint8_t index, bool enabled) {
    if (!isValidIndex(index)) {
        Serial.printf("BatteryManager: Invalid battery index %d\n", index);
        return;
    }

    batteries[index].setEnabled(enabled);
    Serial.printf("BatteryManager: Battery %d %s\n", index, enabled ? "enabled" : "disabled");
}

void BatteryManager::setBatteryName(uint8_t index, const char* name) {
    if (!isValidIndex(index)) {
        Serial.printf("BatteryManager: Invalid battery index %d\n", index);
        return;
    }

    batteries[index].setName(name);
    Serial.printf("BatteryManager: Battery %d renamed to '%s'\n", index, name);
}

void BatteryManager::calibrateCurrent(uint8_t index) {
    if (!isValidIndex(index)) {
        Serial.printf("BatteryManager: Invalid battery index %d\n", index);
        return;
    }

    // Zero-current calibration
    // This would be implemented with the sensor module
    // For now, just a placeholder

    Serial.printf("BatteryManager: Calibrating current sensor for battery %d\n", index);
    Serial.println("BatteryManager: Ensure battery is disconnected (zero current)");

    // TODO: Implement actual calibration with sensor module
    // - Read current sensor value
    // - Store as zero-point offset
    // - Update settings

    Serial.println("BatteryManager: Calibration placeholder - implement with sensors");
}

bool BatteryManager::allBatteriesHealthy() const {
    for (uint8_t i = 0; i < active_count; i++) {
        if (batteries[i].isEnabled()) {
            // Check if battery has errors or stale data
            if (batteries[i].hasError() || !batteries[i].isDataFresh(10000)) {
                return false;
            }

            // Check for error flags in status
            uint8_t flags = batteries[i].getStatusFlags();
            if (flags & CANStatusFlags::ERROR) {
                return false;
            }
        }
    }

    return true;
}

uint8_t BatteryManager::getErrorCount() const {
    uint8_t count = 0;

    for (uint8_t i = 0; i < active_count; i++) {
        if (batteries[i].isEnabled()) {
            if (batteries[i].hasError() || !batteries[i].isDataFresh(10000)) {
                count++;
            }

            // Check for error flags
            uint8_t flags = batteries[i].getStatusFlags();
            if (flags & (CANStatusFlags::ERROR |
                        CANStatusFlags::OVER_VOLTAGE |
                        CANStatusFlags::UNDER_VOLTAGE |
                        CANStatusFlags::OVER_CURRENT |
                        CANStatusFlags::TEMP_WARNING)) {
                count++;
            }
        }
    }

    return count;
}
