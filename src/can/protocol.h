#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>
#include <cstdint>

// Maximum limits for static allocation
// Reduced to minimize DRAM usage
#define MAX_PROTOCOL_NAME_LEN 32
#define MAX_FIELD_NAME_LEN 24
#define MAX_UNIT_LEN 8
#define MAX_FORMULA_LEN 24
#define MAX_ENUM_VALUES 8
#define MAX_FIELDS_PER_MESSAGE 8
#define MAX_MESSAGES_PER_PROTOCOL 8

namespace Protocol {

// Supported data types for CAN message fields
enum class DataType : uint8_t {
    UINT8,
    INT8,
    UINT16_LE,
    UINT16_BE,
    INT16_LE,
    INT16_BE,
    UINT32_LE,
    UINT32_BE,
    INT32_LE,
    INT32_BE,
    FLOAT_LE,
    FLOAT_BE,
    UNKNOWN
};

// Enumeration value mapping (for state machines, etc.)
struct EnumValue {
    uint32_t raw_value;
    char name[MAX_FIELD_NAME_LEN];
};

// Field definition within a CAN message
struct Field {
    char name[MAX_FIELD_NAME_LEN];
    char description[32];  // Reduced from 64 to save DRAM
    uint8_t byte_offset;        // 0-7
    uint8_t length;             // 1, 2, or 4 bytes
    DataType data_type;
    char unit[MAX_UNIT_LEN];    // e.g., "mV", "mA", "°C"
    float scale;                // Multiplication factor
    float offset;               // Additive offset
    char formula[MAX_FORMULA_LEN]; // Optional formula description
    float min_value;
    float max_value;
    bool has_min;               // Whether min_value is valid
    bool has_max;               // Whether max_value is valid

    // Enumeration support
    uint8_t enum_count;
    EnumValue enum_values[MAX_ENUM_VALUES];

    // Extract value from raw CAN data
    float extractValue(const uint8_t* data) const;

    // Check if value is within valid range
    bool isValueValid(float value) const;

    // Get enum name for raw value (returns nullptr if not found)
    const char* getEnumName(uint32_t raw_value) const;
};

// CAN message definition
struct Message {
    uint32_t can_id;            // CAN message ID
    char name[MAX_FIELD_NAME_LEN];
    char description[32];  // Reduced from 64 to save DRAM
    uint16_t period_ms;         // Expected message period

    uint8_t field_count;
    Field fields[MAX_FIELDS_PER_MESSAGE];

    // Find field by name
    const Field* findField(const char* name) const;
};

// Complete protocol definition
struct Definition {
    char name[MAX_PROTOCOL_NAME_LEN];
    char manufacturer[MAX_FIELD_NAME_LEN];
    char version[16];
    char description[48];  // Reduced from 128 to save DRAM
    uint8_t cell_count;
    float nominal_voltage;
    float capacity_ah;
    char chemistry[16];

    uint8_t message_count;
    Message messages[MAX_MESSAGES_PER_PROTOCOL];

    // Find message by CAN ID
    const Message* findMessage(uint32_t can_id) const;

    // Validation
    bool isValid() const;
};

// Helper functions for data type handling
const char* dataTypeToString(DataType type);
DataType stringToDataType(const char* str);
uint8_t getDataTypeSize(DataType type);

// Value extraction helpers
uint16_t extractUint16LE(const uint8_t* data, uint8_t offset);
uint16_t extractUint16BE(const uint8_t* data, uint8_t offset);
int16_t extractInt16LE(const uint8_t* data, uint8_t offset);
int16_t extractInt16BE(const uint8_t* data, uint8_t offset);
uint32_t extractUint32LE(const uint8_t* data, uint8_t offset);
uint32_t extractUint32BE(const uint8_t* data, uint8_t offset);
int32_t extractInt32LE(const uint8_t* data, uint8_t offset);
int32_t extractInt32BE(const uint8_t* data, uint8_t offset);
float extractFloatLE(const uint8_t* data, uint8_t offset);
float extractFloatBE(const uint8_t* data, uint8_t offset);

} // namespace Protocol

#endif // PROTOCOL_H
