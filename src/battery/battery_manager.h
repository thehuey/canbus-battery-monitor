#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>
#include "battery_module.h"
#include "../config/config.h"

// Multi-battery orchestration
class BatteryManager {
public:
    BatteryManager();

    // Initialization
    void begin(uint8_t num_batteries);

    // Update all batteries
    void update();

    // Access battery modules
    BatteryModule* getBattery(uint8_t index);
    const BatteryModule* getBattery(uint8_t index) const;
    uint8_t getActiveBatteryCount() const { return active_count; }

    // Aggregate calculations
    float getTotalPower() const;
    float getTotalCurrent() const;
    float getAverageVoltage() const;

    // Configuration
    void enableBattery(uint8_t index, bool enabled);
    void setBatteryName(uint8_t index, const char* name);
    void calibrateCurrent(uint8_t index);

    // Status checks
    bool allBatteriesHealthy() const;
    uint8_t getErrorCount() const;

private:
    BatteryModule batteries[MAX_BATTERY_MODULES];
    uint8_t active_count;

    // Validation
    bool isValidIndex(uint8_t index) const {
        return index < MAX_BATTERY_MODULES;
    }
};

#endif // BATTERY_MANAGER_H
