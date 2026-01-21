#ifndef PROTOCOL_LOADER_H
#define PROTOCOL_LOADER_H

#include "protocol.h"
#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>

namespace Protocol {

// Protocol loader class
class Loader {
public:
    Loader();

    // Initialize loader (mounts SPIFFS if needed)
    bool begin();

    // Load protocol from JSON file in SPIFFS
    bool loadFromFile(const char* filepath, Definition& protocol);

    // Load protocol from JSON string
    bool loadFromString(const char* json_str, Definition& protocol);

    // Save protocol to SPIFFS
    bool saveToFile(const char* filepath, const Definition& protocol);

    // Fetch protocol from URL and save to SPIFFS
    // Returns true if successful, saves to filepath
    bool fetchFromUrl(const char* url, const char* filepath);

    // List all custom protocols in SPIFFS
    struct ProtocolInfo {
        char filename[32];
        char name[MAX_PROTOCOL_NAME_LEN];
        char manufacturer[MAX_FIELD_NAME_LEN];
        uint32_t file_size;
    };
    uint8_t listCustomProtocols(ProtocolInfo* protocols, uint8_t max_count);

    // Delete a custom protocol file
    bool deleteProtocol(const char* filepath);

    // Validate protocol structure
    bool validate(const Definition& protocol);

    // Get last error message
    const char* getLastError() const { return last_error; }

private:
    char last_error[128];

    // JSON parsing helpers
    bool parseProtocol(const char* json_str, Definition& protocol);
    bool parseMessage(void* json_msg, Message& message);
    bool parseField(void* json_field, Field& field);
    bool parseEnumValues(void* json_enum, Field& field);

    void setError(const char* error);
};

} // namespace Protocol

#endif // PROTOCOL_LOADER_H
