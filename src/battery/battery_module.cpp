#include "battery_module.h"

BatteryModule::BatteryModule()
    : id(0),
      enabled(false),
      voltage(0.0f),
      current(0.0f),
      soc(0),
      max_soc(0),
      temp1(0.0f),
      temp2(0.0f),
      status_flags(0),
      pack_identifier(0),
      cell_count(0),
      has_can_data(false),
      error(false),
      last_update(0) {
    memset(name, 0, sizeof(name));
    memset(bms_info, 0, sizeof(bms_info));
    memset(cell_voltages, 0, sizeof(cell_voltages));
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

    uint16_t fields = can_data.updated_fields;

    // Only update fields that were actually populated by the parser
    if (fields & FIELD_VOLTAGE) {
        voltage = can_data.pack_voltage;
    }
    if (fields & FIELD_CURRENT) {
        current = can_data.pack_current;
    }
    if (fields & FIELD_SOC) {
        soc = can_data.soc;
    }
    if (fields & FIELD_MAX_SOC) {
        max_soc = can_data.max_soc;
    }
    if (fields & FIELD_TEMP1) {
        temp1 = can_data.temp1;
    }
    if (fields & FIELD_TEMP2) {
        temp2 = can_data.temp2;
    }
    if (fields & FIELD_STATUS) {
        status_flags = can_data.status_flags;
    }
    if (fields & FIELD_PACK_ID) {
        pack_identifier = can_data.pack_identifier;
    }
    if (fields & FIELD_BMS_INFO) {
        memcpy(bms_info, can_data.bms_info, sizeof(bms_info));
    }
    if (fields & FIELD_CELL_VOLTS) {
        uint8_t idx = can_data.cell_index;
        if (idx < MAX_CELL_COUNT) {
            cell_voltages[idx] = can_data.cell_voltages[idx];
            if (idx >= cell_count) {
                cell_count = idx + 1;
            }
        }
    }

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
