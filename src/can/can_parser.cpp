#include "can_parser.h"

CANParser::CANParser() : handler_count(0) {
    // Initialize handler registry
    for (size_t i = 0; i < MAX_HANDLERS; i++) {
        handlers[i].can_id = 0;
        handlers[i].handler = nullptr;
    }
}

bool CANParser::parseMessage(const CANMessage& msg, CANBatteryData& data) {
    // Clear previous data
    data = CANBatteryData();

    // Check for registered custom handlers first
    for (size_t i = 0; i < handler_count; i++) {
        if (handlers[i].can_id == msg.id && handlers[i].handler != nullptr) {
            return handlers[i].handler(msg, data);
        }
    }

    // Try built-in parsers based on CAN ID
    // Note: These are example parsers based on the spec
    // Update these as you reverse-engineer the actual protocol

    if (msg.id == 0x100 || (msg.id >= 0x100 && msg.id <= 0x104)) {
        // Battery status messages (0x100 for battery 1, 0x101 for battery 2, etc.)
        return parseBatteryStatus(msg, data);
    } else if (msg.id >= 0x200 && msg.id <= 0x204) {
        // Cell voltage messages (if applicable)
        return parseCellVoltages(msg, data);
    }

    // Unknown message
    return false;
}

void CANParser::registerHandler(uint32_t can_id, MessageHandler handler) {
    if (handler_count >= MAX_HANDLERS) {
        Serial.println("CANParser: Handler registry full!");
        return;
    }

    // Check if handler already exists for this ID
    for (size_t i = 0; i < handler_count; i++) {
        if (handlers[i].can_id == can_id) {
            Serial.printf("CANParser: Updating handler for ID 0x%03X\n", can_id);
            handlers[i].handler = handler;
            return;
        }
    }

    // Add new handler
    handlers[handler_count].can_id = can_id;
    handlers[handler_count].handler = handler;
    handler_count++;

    Serial.printf("CANParser: Registered handler for ID 0x%03X\n", can_id);
}

bool CANParser::parseBatteryStatus(const CANMessage& msg, CANBatteryData& data) {
    // Example parser based on the specification
    // Adjust according to actual battery protocol

    if (msg.dlc < 8) {
        return false;  // Need at least 8 bytes
    }

    // Determine battery ID from CAN message ID
    // Assuming 0x100 = battery 0, 0x101 = battery 1, etc.
    data.battery_id = msg.id - 0x100;

    // Parse pack voltage (bytes 0-1, little-endian, in 0.1V units)
    uint16_t voltage_raw = extractUint16LE(msg.data, 0);
    data.pack_voltage = voltage_raw * 0.1f;

    // Parse pack current (bytes 2-3, signed, offset by 32000, in 0.1A units)
    int16_t current_raw = extractInt16LE(msg.data, 2);
    data.pack_current = (current_raw - 32000) * 0.1f;

    // Parse SOC (byte 4, 0-100%)
    data.soc = msg.data[4];

    // Parse temperatures (bytes 5-6, offset by 40)
    if (msg.data[5] != 0xFF) {  // 0xFF = invalid/not present
        data.temp1 = msg.data[5] - 40.0f;
    }
    if (msg.data[6] != 0xFF) {
        data.temp2 = msg.data[6] - 40.0f;
    }

    // Parse status flags (byte 7)
    data.status_flags = msg.data[7];

    data.valid = true;
    return true;
}

bool CANParser::parseCellVoltages(const CANMessage& msg, CANBatteryData& data) {
    // Example parser for cell voltage messages
    // This is a placeholder - adjust based on actual protocol

    if (msg.dlc < 8) {
        return false;
    }

    // Determine battery ID
    data.battery_id = msg.id - 0x200;

    // Cell voltages are typically in mV, stored as uint16_t
    // This example just marks the data as valid but doesn't extract cell voltages
    // since the CANBatteryData struct doesn't include individual cell data

    data.valid = true;
    return true;
}

uint16_t CANParser::extractUint16LE(const uint8_t* data, uint8_t offset) {
    return (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
}

int16_t CANParser::extractInt16LE(const uint8_t* data, uint8_t offset) {
    return (int16_t)extractUint16LE(data, offset);
}

uint32_t CANParser::extractUint32LE(const uint8_t* data, uint8_t offset) {
    return (uint32_t)data[offset] |
           ((uint32_t)data[offset + 1] << 8) |
           ((uint32_t)data[offset + 2] << 16) |
           ((uint32_t)data[offset + 3] << 24);
}
