#include "can_parser.h"
#include <cmath>

CANParser::CANParser() : protocol(nullptr), cell_voltage_counter(0), handler_count(0) {
    // Initialize handler registry
    for (size_t i = 0; i < MAX_HANDLERS; i++) {
        handlers[i].can_id = 0;
        handlers[i].handler = nullptr;
    }
}

void CANParser::setProtocol(const Protocol::Definition* proto) {
    protocol = proto;
    if (protocol) {
        Serial.printf("CANParser: Protocol set to '%s'\n", protocol->name);
    } else {
        Serial.println("CANParser: Protocol cleared");
    }
}

bool CANParser::parseMessage(const CANMessage& msg, CANBatteryData& data) {
    // Clear previous data
    data = CANBatteryData();

    // Check for registered custom handlers first (legacy support)
    for (size_t i = 0; i < handler_count; i++) {
        if (handlers[i].can_id == msg.id && handlers[i].handler != nullptr) {
            return handlers[i].handler(msg, data);
        }
    }

    // Use protocol-based parsing if protocol is configured
    if (protocol != nullptr) {
        return parseWithProtocol(msg, data);
    }

    // Fall back to legacy parsers if no protocol configured
    if (msg.id == 0x100 || (msg.id >= 0x100 && msg.id <= 0x104)) {
        return parseBatteryStatus(msg, data);
    } else if (msg.id >= 0x200 && msg.id <= 0x204) {
        return parseCellVoltages(msg, data);
    }

    // Unknown message
    return false;
}

bool CANParser::parseWithProtocol(const CANMessage& msg, CANBatteryData& data) {
    // Find the message definition for this CAN ID
    const Protocol::Message* msg_def = protocol->findMessage(msg.id);
    if (!msg_def) {
        return false;  // Message ID not in protocol
    }

    // Extract well-known fields into CANBatteryData structure
    data.valid = true;

    for (uint8_t i = 0; i < msg_def->field_count; i++) {
        const Protocol::Field& field = msg_def->fields[i];

        // Special handling for bms_info (ASCII text, not numeric)
        if (strcmp(field.name, "bms_info") == 0) {
            uint8_t len = field.length;
            if (len > 8) len = 8;
            if (len > msg.dlc) len = msg.dlc;
            memcpy(data.bms_info, msg.data + field.byte_offset, len);
            data.bms_info[len] = '\0';
            data.updated_fields |= FIELD_BMS_INFO;
            continue;
        }

        // Special handling for cell_voltage_mv (series of 13 sequential messages)
        if (strcmp(field.name, "cell_voltage_mv") == 0) {
            float value = field.extractValue(msg.data);
            if (!isnan(value)) {
                uint8_t idx = cell_voltage_counter;
                if (idx >= MAX_CELL_COUNT) {
                    idx = 0;
                    cell_voltage_counter = 0;
                }
                data.cell_voltages[idx] = static_cast<uint16_t>(value);
                data.cell_index = idx;
                data.updated_fields |= FIELD_CELL_VOLTS;
                cell_voltage_counter++;
                if (cell_voltage_counter >= MAX_CELL_COUNT) {
                    cell_voltage_counter = 0;
                }
            }
            continue;
        }

        float value = field.extractValue(msg.data);

        if (isnan(value)) continue;
        if (!field.isValueValid(value)) continue;

        // Map common field names to CANBatteryData fields
        if (strcmp(field.name, "pack_voltage") == 0 ||
            strcmp(field.name, "total_voltage_mv") == 0 ||
            strcmp(field.name, "pack_voltage_mv") == 0) {
            if (strcmp(field.unit, "mV") == 0) {
                data.pack_voltage = value / 1000.0f;
            } else {
                data.pack_voltage = value;
            }
            data.updated_fields |= FIELD_VOLTAGE;
        }
        else if (strcmp(field.name, "pack_current") == 0) {
            if (strcmp(field.unit, "mA") == 0) {
                data.pack_current = value / 1000.0f;
            } else {
                data.pack_current = value;
            }
            data.updated_fields |= FIELD_CURRENT;
        }
        else if (strcmp(field.name, "soc") == 0) {
            data.soc = static_cast<uint16_t>(value);
            data.updated_fields |= FIELD_SOC;
        }
        else if (strcmp(field.name, "max_soc") == 0) {
            data.max_soc = static_cast<uint16_t>(value);
            data.updated_fields |= FIELD_MAX_SOC;
        }
        else if (strcmp(field.name, "temperature") == 0 || strcmp(field.name, "temp1") == 0) {
            data.temp1 = value;
            data.updated_fields |= FIELD_TEMP1;
        }
        else if (strcmp(field.name, "temp2") == 0) {
            data.temp2 = value;
            data.updated_fields |= FIELD_TEMP2;
        }
        else if (strcmp(field.name, "state") == 0 || strcmp(field.name, "status_flags") == 0) {
            data.status_flags = static_cast<uint8_t>(value);
            data.updated_fields |= FIELD_STATUS;
        }
        else if (strcmp(field.name, "pack_identifier") == 0) {
            data.pack_identifier = static_cast<uint32_t>(value);
            data.updated_fields |= FIELD_PACK_ID;
        }
    }

    return true;
}

float CANParser::extractField(const CANMessage& msg, const char* field_name) {
    if (!protocol) {
        return NAN;
    }

    // Find message definition
    const Protocol::Message* msg_def = protocol->findMessage(msg.id);
    if (!msg_def) {
        return NAN;
    }

    // Find field in message
    const Protocol::Field* field = msg_def->findField(field_name);
    if (!field) {
        return NAN;
    }

    // Extract and return value
    return field->extractValue(msg.data);
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
    // Legacy parser - now uses Protocol namespace functions
    // Kept for backwards compatibility when no protocol is configured

    if (msg.dlc < 8) {
        return false;  // Need at least 8 bytes
    }

    // Determine battery ID from CAN message ID
    data.battery_id = msg.id - 0x100;

    // Parse pack voltage (bytes 0-1, little-endian, in 0.1V units)
    uint16_t voltage_raw = Protocol::extractUint16LE(msg.data, 0);
    data.pack_voltage = voltage_raw * 0.1f;
    data.updated_fields |= FIELD_VOLTAGE;

    // Parse pack current (bytes 2-3, signed, offset by 32000, in 0.1A units)
    int16_t current_raw = Protocol::extractInt16LE(msg.data, 2);
    data.pack_current = (current_raw - 32000) * 0.1f;
    data.updated_fields |= FIELD_CURRENT;

    // Parse SOC (byte 4, 0-100%)
    data.soc = msg.data[4];
    data.updated_fields |= FIELD_SOC;

    // Parse temperatures (bytes 5-6, offset by 40)
    if (msg.data[5] != 0xFF) {
        data.temp1 = msg.data[5] - 40.0f;
        data.updated_fields |= FIELD_TEMP1;
    }
    if (msg.data[6] != 0xFF) {
        data.temp2 = msg.data[6] - 40.0f;
        data.updated_fields |= FIELD_TEMP2;
    }

    // Parse status flags (byte 7)
    data.status_flags = msg.data[7];
    data.updated_fields |= FIELD_STATUS;

    data.valid = true;
    return true;
}

bool CANParser::parseCellVoltages(const CANMessage& msg, CANBatteryData& data) {
    // Legacy parser for cell voltage messages
    // Kept for backwards compatibility when no protocol is configured

    if (msg.dlc < 8) {
        return false;
    }

    // Determine battery ID
    data.battery_id = msg.id - 0x200;

    data.valid = true;
    return true;
}
