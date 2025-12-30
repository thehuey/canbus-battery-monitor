#ifndef CAN_MESSAGE_H
#define CAN_MESSAGE_H

#include <Arduino.h>

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

// Parsed battery data from CAN
struct CANBatteryData {
    uint8_t battery_id;
    float pack_voltage;         // Volts
    float pack_current;         // Amps (signed)
    uint8_t soc;                // State of charge (0-100%)
    float temp1;                // Temperature sensor 1 (°C)
    float temp2;                // Temperature sensor 2 (°C)
    uint8_t status_flags;       // Status bits
    bool valid;                 // Data is valid

    CANBatteryData() : battery_id(0), pack_voltage(0), pack_current(0),
                       soc(0), temp1(0), temp2(0), status_flags(0), valid(false) {}
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
