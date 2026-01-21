#include "protocol.h"
#include <cstring>
#include <cmath>

namespace Protocol {

// Field implementation
float Field::extractValue(const uint8_t* data) const {
    if (data == nullptr) return NAN;

    uint32_t raw_value = 0;
    int32_t signed_value = 0;
    float float_value = 0.0f;

    switch (data_type) {
        case DataType::UINT8:
            raw_value = data[byte_offset];
            break;

        case DataType::INT8:
            signed_value = (int8_t)data[byte_offset];
            return (signed_value * scale) + offset;

        case DataType::UINT16_LE:
            raw_value = extractUint16LE(data, byte_offset);
            break;

        case DataType::UINT16_BE:
            raw_value = extractUint16BE(data, byte_offset);
            break;

        case DataType::INT16_LE:
            signed_value = extractInt16LE(data, byte_offset);
            return (signed_value * scale) + offset;

        case DataType::INT16_BE:
            signed_value = extractInt16BE(data, byte_offset);
            return (signed_value * scale) + offset;

        case DataType::UINT32_LE:
            raw_value = extractUint32LE(data, byte_offset);
            break;

        case DataType::UINT32_BE:
            raw_value = extractUint32BE(data, byte_offset);
            break;

        case DataType::INT32_LE:
            signed_value = extractInt32LE(data, byte_offset);
            return (signed_value * scale) + offset;

        case DataType::INT32_BE:
            signed_value = extractInt32BE(data, byte_offset);
            return (signed_value * scale) + offset;

        case DataType::FLOAT_LE:
            float_value = extractFloatLE(data, byte_offset);
            return (float_value * scale) + offset;

        case DataType::FLOAT_BE:
            float_value = extractFloatBE(data, byte_offset);
            return (float_value * scale) + offset;

        default:
            return NAN;
    }

    // For unsigned types, apply scale and offset
    return (raw_value * scale) + offset;
}

bool Field::isValueValid(float value) const {
    if (isnan(value)) return false;
    if (has_min && value < min_value) return false;
    if (has_max && value > max_value) return false;
    return true;
}

const char* Field::getEnumName(uint32_t raw_value) const {
    for (uint8_t i = 0; i < enum_count; i++) {
        if (enum_values[i].raw_value == raw_value) {
            return enum_values[i].name;
        }
    }
    return nullptr;
}

// Message implementation
const Field* Message::findField(const char* name) const {
    for (uint8_t i = 0; i < field_count; i++) {
        if (strcmp(fields[i].name, name) == 0) {
            return &fields[i];
        }
    }
    return nullptr;
}

// Definition implementation
const Message* Definition::findMessage(uint32_t can_id) const {
    for (uint8_t i = 0; i < message_count; i++) {
        if (messages[i].can_id == can_id) {
            return &messages[i];
        }
    }
    return nullptr;
}

bool Definition::isValid() const {
    // Check basic fields
    if (strlen(name) == 0) return false;
    if (message_count == 0) return false;
    if (message_count > MAX_MESSAGES_PER_PROTOCOL) return false;

    // Validate each message
    for (uint8_t i = 0; i < message_count; i++) {
        const Message& msg = messages[i];

        if (msg.field_count == 0) return false;
        if (msg.field_count > MAX_FIELDS_PER_MESSAGE) return false;

        // Validate each field
        for (uint8_t j = 0; j < msg.field_count; j++) {
            const Field& field = msg.fields[j];

            // Check byte offset is within CAN frame
            if (field.byte_offset >= 8) return false;
            if (field.byte_offset + field.length > 8) return false;

            // Check data type size matches length
            uint8_t expected_size = getDataTypeSize(field.data_type);
            if (expected_size != field.length) return false;

            // Check scale is not zero
            if (field.scale == 0.0f) return false;
        }
    }

    return true;
}

// Data type helpers
const char* dataTypeToString(DataType type) {
    switch (type) {
        case DataType::UINT8: return "uint8";
        case DataType::INT8: return "int8";
        case DataType::UINT16_LE: return "uint16_le";
        case DataType::UINT16_BE: return "uint16_be";
        case DataType::INT16_LE: return "int16_le";
        case DataType::INT16_BE: return "int16_be";
        case DataType::UINT32_LE: return "uint32_le";
        case DataType::UINT32_BE: return "uint32_be";
        case DataType::INT32_LE: return "int32_le";
        case DataType::INT32_BE: return "int32_be";
        case DataType::FLOAT_LE: return "float_le";
        case DataType::FLOAT_BE: return "float_be";
        default: return "unknown";
    }
}

DataType stringToDataType(const char* str) {
    if (strcmp(str, "uint8") == 0) return DataType::UINT8;
    if (strcmp(str, "int8") == 0) return DataType::INT8;
    if (strcmp(str, "uint16_le") == 0) return DataType::UINT16_LE;
    if (strcmp(str, "uint16_be") == 0) return DataType::UINT16_BE;
    if (strcmp(str, "int16_le") == 0) return DataType::INT16_LE;
    if (strcmp(str, "int16_be") == 0) return DataType::INT16_BE;
    if (strcmp(str, "uint32_le") == 0) return DataType::UINT32_LE;
    if (strcmp(str, "uint32_be") == 0) return DataType::UINT32_BE;
    if (strcmp(str, "int32_le") == 0) return DataType::INT32_LE;
    if (strcmp(str, "int32_be") == 0) return DataType::INT32_BE;
    if (strcmp(str, "float_le") == 0) return DataType::FLOAT_LE;
    if (strcmp(str, "float_be") == 0) return DataType::FLOAT_BE;
    return DataType::UNKNOWN;
}

uint8_t getDataTypeSize(DataType type) {
    switch (type) {
        case DataType::UINT8:
        case DataType::INT8:
            return 1;

        case DataType::UINT16_LE:
        case DataType::UINT16_BE:
        case DataType::INT16_LE:
        case DataType::INT16_BE:
            return 2;

        case DataType::UINT32_LE:
        case DataType::UINT32_BE:
        case DataType::INT32_LE:
        case DataType::INT32_BE:
        case DataType::FLOAT_LE:
        case DataType::FLOAT_BE:
            return 4;

        default:
            return 0;
    }
}

// Value extraction implementations
uint16_t extractUint16LE(const uint8_t* data, uint8_t offset) {
    return (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
}

uint16_t extractUint16BE(const uint8_t* data, uint8_t offset) {
    return ((uint16_t)data[offset] << 8) | (uint16_t)data[offset + 1];
}

int16_t extractInt16LE(const uint8_t* data, uint8_t offset) {
    return (int16_t)extractUint16LE(data, offset);
}

int16_t extractInt16BE(const uint8_t* data, uint8_t offset) {
    return (int16_t)extractUint16BE(data, offset);
}

uint32_t extractUint32LE(const uint8_t* data, uint8_t offset) {
    return (uint32_t)data[offset] |
           ((uint32_t)data[offset + 1] << 8) |
           ((uint32_t)data[offset + 2] << 16) |
           ((uint32_t)data[offset + 3] << 24);
}

uint32_t extractUint32BE(const uint8_t* data, uint8_t offset) {
    return ((uint32_t)data[offset] << 24) |
           ((uint32_t)data[offset + 1] << 16) |
           ((uint32_t)data[offset + 2] << 8) |
           (uint32_t)data[offset + 3];
}

int32_t extractInt32LE(const uint8_t* data, uint8_t offset) {
    return (int32_t)extractUint32LE(data, offset);
}

int32_t extractInt32BE(const uint8_t* data, uint8_t offset) {
    return (int32_t)extractUint32BE(data, offset);
}

float extractFloatLE(const uint8_t* data, uint8_t offset) {
    uint32_t bits = extractUint32LE(data, offset);
    float result;
    memcpy(&result, &bits, sizeof(float));
    return result;
}

float extractFloatBE(const uint8_t* data, uint8_t offset) {
    uint32_t bits = extractUint32BE(data, offset);
    float result;
    memcpy(&result, &bits, sizeof(float));
    return result;
}

} // namespace Protocol
