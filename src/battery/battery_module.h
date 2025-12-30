#ifndef BATTERY_MODULE_H
#define BATTERY_MODULE_H

#include <Arduino.h>
#include "../can/can_message.h"

// Single battery module data and state
class BatteryModule {
public:
    BatteryModule();

    // Initialization
    void begin(uint8_t id, const char* name);

    // Update sensor readings
    void updateVoltage(float voltage);
    void updateCurrent(float current);
    void updateFromCAN(const CANBatteryData& can_data);

    // Getters
    uint8_t getId() const { return id; }
    const char* getName() const { return name; }
    bool isEnabled() const { return enabled; }
    float getVoltage() const { return voltage; }
    float getCurrent() const { return current; }
    float getPower() const { return voltage * current; }
    uint8_t getSOC() const { return soc; }
    float getTemp1() const { return temp1; }
    float getTemp2() const { return temp2; }
    uint8_t getStatusFlags() const { return status_flags; }
    uint32_t getLastUpdate() const { return last_update; }
    bool hasError() const { return error; }

    // Setters
    void setEnabled(bool enable) { enabled = enable; }
    void setName(const char* new_name);
    void setError(bool err) { error = err; }

    // Status checks
    bool isDataFresh(uint32_t timeout_ms = 5000) const;
    bool hasCANData() const { return has_can_data; }

private:
    uint8_t id;
    char name[16];
    bool enabled;

    // Sensor data
    float voltage;              // Volts
    float current;              // Amps
    uint8_t soc;                // State of charge (%)
    float temp1;                // Temperature 1 (°C)
    float temp2;                // Temperature 2 (°C)
    uint8_t status_flags;       // Status bits from CAN

    // State
    bool has_can_data;
    bool error;
    uint32_t last_update;       // Timestamp of last data update
};

#endif // BATTERY_MODULE_H
