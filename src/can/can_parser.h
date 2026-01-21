#ifndef CAN_PARSER_H
#define CAN_PARSER_H

#include "can_message.h"
#include "protocol.h"

// CAN protocol parser class
class CANParser {
public:
    CANParser();

    // Set the protocol to use for parsing
    void setProtocol(const Protocol::Definition* protocol);

    // Parse a CAN message and extract battery data using the configured protocol
    bool parseMessage(const CANMessage& msg, CANBatteryData& data);

    // Extract a specific field value from a CAN message
    // Returns NAN if field not found or extraction failed
    float extractField(const CANMessage& msg, const char* field_name);

    // Get the current protocol
    const Protocol::Definition* getProtocol() const { return protocol; }

    // Register custom message handlers (legacy support)
    typedef bool (*MessageHandler)(const CANMessage&, CANBatteryData&);
    void registerHandler(uint32_t can_id, MessageHandler handler);

private:
    const Protocol::Definition* protocol;

    // Parse message using protocol definition
    bool parseWithProtocol(const CANMessage& msg, CANBatteryData& data);

    // Legacy parsers for backwards compatibility (deprecated)
    bool parseBatteryStatus(const CANMessage& msg, CANBatteryData& data);
    bool parseCellVoltages(const CANMessage& msg, CANBatteryData& data);

    // Custom handlers registry (legacy support)
    static constexpr size_t MAX_HANDLERS = 16;
    struct HandlerEntry {
        uint32_t can_id;
        MessageHandler handler;
    };
    HandlerEntry handlers[MAX_HANDLERS];
    size_t handler_count;
};

#endif // CAN_PARSER_H
