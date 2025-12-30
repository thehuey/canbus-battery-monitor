#include "battery_module.h"

BatteryModule::BatteryModule()
    : id(0),
      enabled(false),
      voltage(0.0f),
      current(0.0f),
      soc(0),
      temp1(0.0f),
      temp2(0.0f),
      status_flags(0),
      has_can_data(false),
      error(false),
      last_update(0) {
    memset(name, 0, sizeof(name));
}

void BatteryModule::begin(uint8_t id, const char* name) {
    this->id = id;
    setName(name);
    enabled = true;
    last_update = millis();

    Serial.printf("BatteryModule %d (%s): Initialized\n", id, this->name);
}

void BatteryModule::updateVoltage(float voltage) {
    if (!enabled) return;

    this->voltage = voltage;
    last_update = millis();
}

void BatteryModule::updateCurrent(float current) {
    if (!enabled) return;

    this->current = current;
    last_update = millis();
}

void BatteryModule::updateFromCAN(const CANBatteryData& can_data) {
    if (!enabled) return;

    // Update all fields from CAN data
    voltage = can_data.pack_voltage;
    current = can_data.pack_current;
    soc = can_data.soc;
    temp1 = can_data.temp1;
    temp2 = can_data.temp2;
    status_flags = can_data.status_flags;

    has_can_data = can_data.valid;
    last_update = millis();

    // Clear error if we're receiving valid data
    if (can_data.valid) {
        error = false;
    }
}

void BatteryModule::setName(const char* new_name) {
    if (new_name != nullptr) {
        strlcpy(name, new_name, sizeof(name));
    }
}

bool BatteryModule::isDataFresh(uint32_t timeout_ms) const {
    if (!enabled) {
        return false;
    }

    uint32_t age = millis() - last_update;
    return age < timeout_ms;
}
