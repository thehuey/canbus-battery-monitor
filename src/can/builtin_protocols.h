#ifndef BUILTIN_PROTOCOLS_H
#define BUILTIN_PROTOCOLS_H

#include "protocol.h"

namespace Protocol {

// Built-in protocol IDs
enum class BuiltinId : uint8_t {
    DPOWER_48V_13S = 0,
    GENERIC_BMS = 1,
    COUNT  // Total number of built-in protocols
};

// Get a built-in protocol by ID
const Definition* getBuiltinProtocol(BuiltinId id);

// Get built-in protocol name
const char* getBuiltinProtocolName(BuiltinId id);

// List all built-in protocols
const Definition* const* getAllBuiltinProtocols(uint8_t& count);

} // namespace Protocol

#endif // BUILTIN_PROTOCOLS_H
