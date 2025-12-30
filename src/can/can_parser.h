#ifndef CAN_PARSER_H
#define CAN_PARSER_H

#include "can_message.h"

// CAN protocol parser class
class CANParser {
public:
    CANParser();

    // Parse a CAN message and extract battery data
    bool parseMessage(const CANMessage& msg, CANBatteryData& data);

    // Register custom message handlers
    typedef bool (*MessageHandler)(const CANMessage&, CANBatteryData&);
    void registerHandler(uint32_t can_id, MessageHandler handler);

private:
    // Built-in parsers for known message types
    bool parseBatteryStatus(const CANMessage& msg, CANBatteryData& data);
    bool parseCellVoltages(const CANMessage& msg, CANBatteryData& data);

    // Helper functions for data extraction
    uint16_t extractUint16LE(const uint8_t* data, uint8_t offset);
    int16_t extractInt16LE(const uint8_t* data, uint8_t offset);
    uint32_t extractUint32LE(const uint8_t* data, uint8_t offset);

    // Custom handlers registry
    static constexpr size_t MAX_HANDLERS = 16;
    struct HandlerEntry {
        uint32_t can_id;
        MessageHandler handler;
    };
    HandlerEntry handlers[MAX_HANDLERS];
    size_t handler_count;
};

#endif // CAN_PARSER_H
