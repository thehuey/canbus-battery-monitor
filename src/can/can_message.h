#ifndef CAN_MESSAGE_H
#define CAN_MESSAGE_H

#include <Arduino.h>

#define MAX_CELL_COUNT 13

// CAN frame structure
struct CANMessage {
    uint32_t id;                // CAN identifier
    uint8_t dlc;                // Data length code (0-8)
    uint8_t data[8];            // Data bytes
    uint32_t timestamp;         // Timestamp in milliseconds
    bool extended;              // Extended frame format
    bool rtr;                   // Remote transmission request

    CANMessage() : id(0), dlc(0), timestamp(0), extended(false), rtr(false) {
        memset(data, 0, sizeof(data));
    }
};

// Bitmask indicating which fields were populated by a parse operation
enum CANDataField : uint16_t {
    FIELD_VOLTAGE     = 0x0001,
    FIELD_CURRENT     = 0x0002,
    FIELD_SOC         = 0x0004,
    FIELD_TEMP1       = 0x0008,
    FIELD_TEMP2       = 0x0010,
    FIELD_STATUS      = 0x0020,
    FIELD_PACK_ID     = 0x0040,
    FIELD_BMS_INFO    = 0x0080,
    FIELD_CELL_VOLTS  = 0x0100,
    FIELD_MAX_SOC     = 0x0200,
};

// Parsed battery data from CAN
struct CANBatteryData {
    uint8_t battery_id;
    uint16_t updated_fields;    // Bitmask of CANDataField values
    float pack_voltage;         // Volts
    float pack_current;         // Amps (signed)
    uint16_t soc;               // State of charge (raw value from BMS)
    uint16_t max_soc;           // Maximum SOC capacity (raw value from BMS)
    float temp1;                // Temperature sensor 1 (°C)
    float temp2;                // Temperature sensor 2 (°C)
    uint8_t status_flags;       // Status bits
    uint32_t pack_identifier;   // Manufacturing date/serial (YYDDMMSSSS format)
    char bms_info[9];           // ASCII info from extended CAN message (8 chars + null)
    uint16_t cell_voltages[MAX_CELL_COUNT]; // Per-cell voltages in mV
    uint8_t cell_index;         // Which cell was updated (for series messages)
    bool valid;                 // Data is valid

    CANBatteryData() : battery_id(0), updated_fields(0), pack_voltage(0), pack_current(0),
                       soc(0), max_soc(0), temp1(0), temp2(0), status_flags(0),
                       pack_identifier(0), cell_index(0), valid(false) {
        memset(bms_info, 0, sizeof(bms_info));
        memset(cell_voltages, 0, sizeof(cell_voltages));
    }
};

// Status flag definitions
namespace CANStatusFlags {
    constexpr uint8_t CHARGING       = 0x01;
    constexpr uint8_t DISCHARGING    = 0x02;
    constexpr uint8_t BALANCING      = 0x04;
    constexpr uint8_t TEMP_WARNING   = 0x08;
    constexpr uint8_t OVER_VOLTAGE   = 0x10;
    constexpr uint8_t UNDER_VOLTAGE  = 0x20;
    constexpr uint8_t OVER_CURRENT   = 0x40;
    constexpr uint8_t ERROR          = 0x80;
}

#endif // CAN_MESSAGE_H
