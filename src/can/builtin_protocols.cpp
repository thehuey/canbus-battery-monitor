#include "builtin_protocols.h"
#include <cstring>

namespace Protocol {

// Helper function to create a Field
static Field makeField(const char* name, const char* description, uint8_t byte_offset,
                      uint8_t length, DataType data_type, const char* unit,
                      float scale, float offset, const char* formula = "",
                      float min_value = 0.0f, float max_value = 0.0f,
                      bool has_min = false, bool has_max = false) {
    Field f;
    strncpy(f.name, name, sizeof(f.name) - 1);
    f.name[sizeof(f.name) - 1] = '\0';
    strncpy(f.description, description, sizeof(f.description) - 1);
    f.description[sizeof(f.description) - 1] = '\0';
    f.byte_offset = byte_offset;
    f.length = length;
    f.data_type = data_type;
    strncpy(f.unit, unit, sizeof(f.unit) - 1);
    f.unit[sizeof(f.unit) - 1] = '\0';
    f.scale = scale;
    f.offset = offset;
    strncpy(f.formula, formula, sizeof(f.formula) - 1);
    f.formula[sizeof(f.formula) - 1] = '\0';
    f.min_value = min_value;
    f.max_value = max_value;
    f.has_min = has_min;
    f.has_max = has_max;
    f.enum_count = 0;
    return f;
}

// Helper to add enum value to field
static void addEnumValue(Field& field, uint32_t raw_value, const char* name) {
    if (field.enum_count < MAX_ENUM_VALUES) {
        field.enum_values[field.enum_count].raw_value = raw_value;
        strncpy(field.enum_values[field.enum_count].name, name,
                sizeof(field.enum_values[field.enum_count].name) - 1);
        field.enum_values[field.enum_count].name[sizeof(field.enum_values[field.enum_count].name) - 1] = '\0';
        field.enum_count++;
    }
}

// ============================================================================
// D-power 48V 13S Protocol Definition
// ============================================================================

static Definition createDPowerProtocol() {
    Definition proto;
    strncpy(proto.name, "Tianjin D-power 48V 13S", sizeof(proto.name) - 1);
    strncpy(proto.manufacturer, "D-power", sizeof(proto.manufacturer) - 1);
    strncpy(proto.version, "1.0", sizeof(proto.version) - 1);
    strncpy(proto.description, "48V 13S 25Ah Li-ion battery pack", sizeof(proto.description) - 1);
    strncpy(proto.chemistry, "Li-ion", sizeof(proto.chemistry) - 1);
    proto.cell_count = 13;
    proto.nominal_voltage = 48.0f;
    proto.capacity_ah = 25.0f;
    // message_count set at end after all messages are defined

    // Message 0x201 - Pack Identification
    Message& msg201 = proto.messages[0];
    msg201.can_id = 0x201;
    strncpy(msg201.name, "Pack Identification", sizeof(msg201.name) - 1);
    strncpy(msg201.description, "Manufacturing date and serial", sizeof(msg201.description) - 1);
    msg201.period_ms = 1000;
    msg201.field_count = 1;

    msg201.fields[0] = makeField("pack_identifier", "32-bit ID in YYDDMMSSSS format",
                                 0, 4, DataType::UINT32_LE, "", 1.0f, 0.0f, "",
                                 0.0f, 0.0f, false, false);

    // Message 0x202 - Cell Voltage (series of 13)
    Message& msg202 = proto.messages[1];
    msg202.can_id = 0x202;
    strncpy(msg202.name, "Cell Voltage", sizeof(msg202.name) - 1);
    strncpy(msg202.description, "Cell voltage mV (13 in seq)", sizeof(msg202.description) - 1);
    msg202.period_ms = 100;
    msg202.field_count = 1;

    msg202.fields[0] = makeField("cell_voltage_mv", "Cell voltage in mV (series)",
                                 0, 2, DataType::UINT16_LE, "mV", 1.0f, 0.0f, "",
                                 0.0f, 5000.0f, true, true);

    // Message 0x203 - State of Charge
    Message& msg203 = proto.messages[2];
    msg203.can_id = 0x203;
    strncpy(msg203.name, "State of Charge", sizeof(msg203.name) - 1);
    strncpy(msg203.description, "Current and max SOC in mAh", sizeof(msg203.description) - 1);
    msg203.period_ms = 50;
    msg203.field_count = 2;

    msg203.fields[0] = makeField("soc", "Current SOC in mAh",
                                 0, 2, DataType::UINT16_LE, "mAh", 1.0f, 0.0f, "",
                                 0.0f, 65535.0f, true, true);

    msg203.fields[1] = makeField("max_soc", "Max SOC capacity in mAh",
                                 4, 2, DataType::UINT16_LE, "mAh", 1.0f, 0.0f, "",
                                 0.0f, 65535.0f, true, true);

    // Message 0x204 - Battery State
    Message& msg204 = proto.messages[3];
    msg204.can_id = 0x204;
    strncpy(msg204.name, "Battery State", sizeof(msg204.name) - 1);
    strncpy(msg204.description, "Battery state machine", sizeof(msg204.description) - 1);
    msg204.period_ms = 100;
    msg204.field_count = 1;

    msg204.fields[0] = makeField("state", "Battery state machine",
                                 0, 1, DataType::UINT8, "", 1.0f, 0.0f, "",
                                 0.0f, 255.0f, true, true);

    // Add enum values for state field (observed: 0x00, 0x10, 0x20, 0x21, 0x22)
    addEnumValue(msg204.fields[0], 0, "idle");
    addEnumValue(msg204.fields[0], 16, "standby");
    addEnumValue(msg204.fields[0], 32, "charging_phase_1");
    addEnumValue(msg204.fields[0], 33, "charging_phase_2");
    addEnumValue(msg204.fields[0], 34, "charging_phase_3");

    // Message 0x70C6800 - BMS Information (extended frame)
    Message& msgBMS = proto.messages[4];
    msgBMS.can_id = 0x70C6800;
    strncpy(msgBMS.name, "BMS Information", sizeof(msgBMS.name) - 1);
    strncpy(msgBMS.description, "HW/FW/BMS type ASCII text", sizeof(msgBMS.description) - 1);
    msgBMS.period_ms = 1000;
    msgBMS.field_count = 1;

    msgBMS.fields[0] = makeField("bms_info", "ASCII HW/FW/BMS info string",
                                 0, 8, DataType::UINT8, "", 1.0f, 0.0f, "",
                                 0.0f, 0.0f, false, false);

    // Message 0x12183000 - Sleep Command (extended frame, DLC=0)
    Message& msgSleep = proto.messages[5];
    msgSleep.can_id = 0x12183000;
    strncpy(msgSleep.name, "Sleep Command", sizeof(msgSleep.name) - 1);
    strncpy(msgSleep.description, "Puts battery to sleep (DLC=0)", sizeof(msgSleep.description) - 1);
    msgSleep.period_ms = 0;
    msgSleep.field_count = 0;

    // Message 0x130C6000 - Wake Command (extended frame, DLC=0)
    Message& msgWake = proto.messages[6];
    msgWake.can_id = 0x130C6000;
    strncpy(msgWake.name, "Wake Command", sizeof(msgWake.name) - 1);
    strncpy(msgWake.description, "Wakes battery up (DLC=0)", sizeof(msgWake.description) - 1);
    msgWake.period_ms = 0;
    msgWake.field_count = 0;

    proto.message_count = 7;

    return proto;
}

// ============================================================================
// Generic BMS Protocol Definition
// ============================================================================

#ifndef GENERIC_BMS_DISABLED
static Definition createGenericBMSProtocol() {
    Definition proto;
    strncpy(proto.name, "Generic BMS", sizeof(proto.name) - 1);
    strncpy(proto.manufacturer, "Generic", sizeof(proto.manufacturer) - 1);
    strncpy(proto.version, "1.0", sizeof(proto.version) - 1);
    strncpy(proto.description, "Generic BMS protocol template", sizeof(proto.description) - 1);
    strncpy(proto.chemistry, "Li-ion", sizeof(proto.chemistry) - 1);
    proto.cell_count = 0;
    proto.nominal_voltage = 0.0f;
    proto.capacity_ah = 0.0f;
    proto.message_count = 1;

    // Message 0x100 - Battery Status
    Message& msg100 = proto.messages[0];
    msg100.can_id = 0x100;
    strncpy(msg100.name, "Battery Status", sizeof(msg100.name) - 1);
    strncpy(msg100.description, "Common battery status", sizeof(msg100.description) - 1);
    msg100.period_ms = 100;
    msg100.field_count = 4;

    msg100.fields[0] = makeField("pack_voltage", "Pack voltage",
                                 0, 2, DataType::UINT16_LE, "mV", 0.1f, 0.0f, "",
                                 0.0f, 100000.0f, true, true);

    msg100.fields[1] = makeField("pack_current", "Pack current",
                                 2, 2, DataType::INT16_LE, "mA", 0.1f, -3200.0f, "",
                                 -32000.0f, 32000.0f, true, true);

    msg100.fields[2] = makeField("soc", "State of charge",
                                 4, 1, DataType::UINT8, "%", 1.0f, 0.0f, "",
                                 0.0f, 100.0f, true, true);

    msg100.fields[3] = makeField("temperature", "Battery temperature",
                                 5, 1, DataType::UINT8, "C", 1.0f, -40.0f, "",
                                 -40.0f, 100.0f, true, true);

    return proto;
}
#endif // GENERIC_BMS_DISABLED

// ============================================================================
// Protocol Registry
// ============================================================================

const Definition* getBuiltinProtocol(BuiltinId id) {
    // Use static local variables to avoid storing in DRAM permanently
    // These are created once on first access
    static Definition dpower_protocol;
#ifndef GENERIC_BMS_DISABLED
    static Definition generic_protocol;
#endif
    static bool initialized = false;

    if (!initialized) {
        dpower_protocol = createDPowerProtocol();
#ifndef GENERIC_BMS_DISABLED
        generic_protocol = createGenericBMSProtocol();
#endif
        initialized = true;
    }

    switch (id) {
        case BuiltinId::DPOWER_48V_13S:
            return &dpower_protocol;
#ifndef GENERIC_BMS_DISABLED
        case BuiltinId::GENERIC_BMS:
            return &generic_protocol;
#endif
        default:
            return nullptr;
    }
}

const char* getBuiltinProtocolName(BuiltinId id) {
    const Definition* proto = getBuiltinProtocol(id);
    return proto ? proto->name : nullptr;
}

const Definition* const* getAllBuiltinProtocols(uint8_t& count) {
    // Build array on demand to avoid permanent DRAM usage
#ifdef GENERIC_BMS_DISABLED
    static const Definition* builtin_protocols[1];
#else
    static const Definition* builtin_protocols[static_cast<uint8_t>(BuiltinId::COUNT)];
#endif
    static bool initialized = false;

    if (!initialized) {
        builtin_protocols[0] = getBuiltinProtocol(BuiltinId::DPOWER_48V_13S);
#ifndef GENERIC_BMS_DISABLED
        builtin_protocols[1] = getBuiltinProtocol(BuiltinId::GENERIC_BMS);
#endif
        initialized = true;
    }

#ifdef GENERIC_BMS_DISABLED
    count = 1;
#else
    count = static_cast<uint8_t>(BuiltinId::COUNT);
#endif
    return builtin_protocols;
}

} // namespace Protocol
