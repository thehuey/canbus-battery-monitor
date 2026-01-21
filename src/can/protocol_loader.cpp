#include "protocol_loader.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

namespace Protocol {

Loader::Loader() {
    last_error[0] = '\0';
}

bool Loader::begin() {
    if (!SPIFFS.begin(true)) {
        setError("Failed to mount SPIFFS");
        return false;
    }

    // Ensure protocols directory exists
    if (!SPIFFS.exists("/protocols")) {
        SPIFFS.mkdir("/protocols");
    }

    return true;
}

bool Loader::loadFromFile(const char* filepath, Definition& protocol) {
    File file = SPIFFS.open(filepath, "r");
    if (!file) {
        setError("Failed to open protocol file");
        return false;
    }

    size_t size = file.size();
    if (size == 0 || size > 16384) {  // Max 16KB for protocol file
        file.close();
        setError("Invalid file size");
        return false;
    }

    // Read file into buffer
    char* buffer = new char[size + 1];
    if (!buffer) {
        file.close();
        setError("Memory allocation failed");
        return false;
    }

    file.readBytes(buffer, size);
    buffer[size] = '\0';
    file.close();

    bool result = loadFromString(buffer, protocol);
    delete[] buffer;

    return result;
}

bool Loader::loadFromString(const char* json_str, Definition& protocol) {
    return parseProtocol(json_str, protocol);
}

bool Loader::saveToFile(const char* filepath, const Definition& protocol) {
    // Create JSON document
    JsonDocument doc;

    doc["name"] = protocol.name;
    doc["manufacturer"] = protocol.manufacturer;
    doc["version"] = protocol.version;
    doc["description"] = protocol.description;
    doc["cell_count"] = protocol.cell_count;
    doc["nominal_voltage"] = protocol.nominal_voltage;
    doc["capacity_ah"] = protocol.capacity_ah;
    doc["chemistry"] = protocol.chemistry;

    JsonArray messages = doc["messages"].to<JsonArray>();

    for (uint8_t i = 0; i < protocol.message_count; i++) {
        const Message& msg = protocol.messages[i];
        JsonObject json_msg = messages.add<JsonObject>();

        json_msg["can_id"] = msg.can_id;
        json_msg["name"] = msg.name;
        json_msg["description"] = msg.description;
        json_msg["period_ms"] = msg.period_ms;

        JsonArray fields = json_msg["fields"].to<JsonArray>();

        for (uint8_t j = 0; j < msg.field_count; j++) {
            const Field& field = msg.fields[j];
            JsonObject json_field = fields.add<JsonObject>();

            json_field["name"] = field.name;
            json_field["description"] = field.description;
            json_field["byte_offset"] = field.byte_offset;
            json_field["length"] = field.length;
            json_field["data_type"] = dataTypeToString(field.data_type);
            json_field["unit"] = field.unit;
            json_field["scale"] = field.scale;
            json_field["offset"] = field.offset;

            if (strlen(field.formula) > 0) {
                json_field["formula"] = field.formula;
            }

            if (field.has_min) json_field["min_value"] = field.min_value;
            if (field.has_max) json_field["max_value"] = field.max_value;

            if (field.enum_count > 0) {
                JsonObject enum_obj = json_field["enum_values"].to<JsonObject>();
                for (uint8_t k = 0; k < field.enum_count; k++) {
                    char key[16];
                    snprintf(key, sizeof(key), "%u", field.enum_values[k].raw_value);
                    enum_obj[key] = field.enum_values[k].name;
                }
            }
        }
    }

    // Write to file
    File file = SPIFFS.open(filepath, "w");
    if (!file) {
        setError("Failed to create file");
        return false;
    }

    if (serializeJson(doc, file) == 0) {
        file.close();
        setError("Failed to write JSON");
        return false;
    }

    file.close();
    return true;
}

bool Loader::fetchFromUrl(const char* url, const char* filepath) {
    HTTPClient http;

    if (!http.begin(url)) {
        setError("Failed to connect to URL");
        return false;
    }

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        http.end();
        setError("HTTP request failed");
        return false;
    }

    String payload = http.getString();
    http.end();

    if (payload.length() == 0 || payload.length() > 16384) {
        setError("Invalid response size");
        return false;
    }

    // Parse to validate
    Definition temp_protocol;
    if (!loadFromString(payload.c_str(), temp_protocol)) {
        return false;  // Error already set
    }

    // Save to file
    File file = SPIFFS.open(filepath, "w");
    if (!file) {
        setError("Failed to create file");
        return false;
    }

    file.print(payload);
    file.close();

    return true;
}

uint8_t Loader::listCustomProtocols(ProtocolInfo* protocols, uint8_t max_count) {
    File root = SPIFFS.open("/protocols");
    if (!root || !root.isDirectory()) {
        return 0;
    }

    uint8_t count = 0;
    File file = root.openNextFile();

    while (file && count < max_count) {
        if (!file.isDirectory() && strstr(file.name(), ".json")) {
            // Try to parse protocol name
            Definition temp;
            if (loadFromFile(file.path(), temp)) {
                strncpy(protocols[count].filename, file.name(), sizeof(protocols[count].filename) - 1);
                strncpy(protocols[count].name, temp.name, sizeof(protocols[count].name) - 1);
                strncpy(protocols[count].manufacturer, temp.manufacturer, sizeof(protocols[count].manufacturer) - 1);
                protocols[count].file_size = file.size();
                count++;
            }
        }
        file = root.openNextFile();
    }

    return count;
}

bool Loader::deleteProtocol(const char* filepath) {
    if (!SPIFFS.exists(filepath)) {
        setError("File does not exist");
        return false;
    }

    if (!SPIFFS.remove(filepath)) {
        setError("Failed to delete file");
        return false;
    }

    return true;
}

bool Loader::validate(const Definition& protocol) {
    return protocol.isValid();
}

bool Loader::parseProtocol(const char* json_str, Definition& protocol) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json_str);

    if (error) {
        snprintf(last_error, sizeof(last_error), "JSON parse error: %s", error.c_str());
        return false;
    }

    // Parse top-level fields
    strncpy(protocol.name, doc["name"] | "", sizeof(protocol.name) - 1);
    strncpy(protocol.manufacturer, doc["manufacturer"] | "", sizeof(protocol.manufacturer) - 1);
    strncpy(protocol.version, doc["version"] | "1.0", sizeof(protocol.version) - 1);
    strncpy(protocol.description, doc["description"] | "", sizeof(protocol.description) - 1);
    strncpy(protocol.chemistry, doc["chemistry"] | "Li-ion", sizeof(protocol.chemistry) - 1);

    protocol.cell_count = doc["cell_count"] | 0;
    protocol.nominal_voltage = doc["nominal_voltage"] | 0.0f;
    protocol.capacity_ah = doc["capacity_ah"] | 0.0f;

    // Parse messages
    JsonArray messages = doc["messages"];
    if (!messages) {
        setError("No messages array found");
        return false;
    }

    protocol.message_count = 0;
    for (JsonObject json_msg : messages) {
        if (protocol.message_count >= MAX_MESSAGES_PER_PROTOCOL) {
            setError("Too many messages");
            return false;
        }

        if (!parseMessage(&json_msg, protocol.messages[protocol.message_count])) {
            return false;
        }

        protocol.message_count++;
    }

    // Validate
    if (!validate(protocol)) {
        setError("Protocol validation failed");
        return false;
    }

    return true;
}

bool Loader::parseMessage(void* json_msg_ptr, Message& message) {
    JsonObject json_msg = *static_cast<JsonObject*>(json_msg_ptr);

    message.can_id = json_msg["can_id"];
    strncpy(message.name, json_msg["name"] | "", sizeof(message.name) - 1);
    strncpy(message.description, json_msg["description"] | "", sizeof(message.description) - 1);
    message.period_ms = json_msg["period_ms"] | 100;

    // Parse fields
    JsonArray fields = json_msg["fields"];
    if (!fields) {
        setError("No fields array in message");
        return false;
    }

    message.field_count = 0;
    for (JsonObject json_field : fields) {
        if (message.field_count >= MAX_FIELDS_PER_MESSAGE) {
            setError("Too many fields");
            return false;
        }

        if (!parseField(&json_field, message.fields[message.field_count])) {
            return false;
        }

        message.field_count++;
    }

    return true;
}

bool Loader::parseField(void* json_field_ptr, Field& field) {
    JsonObject json_field = *static_cast<JsonObject*>(json_field_ptr);

    strncpy(field.name, json_field["name"] | "", sizeof(field.name) - 1);
    strncpy(field.description, json_field["description"] | "", sizeof(field.description) - 1);
    strncpy(field.unit, json_field["unit"] | "", sizeof(field.unit) - 1);
    strncpy(field.formula, json_field["formula"] | "", sizeof(field.formula) - 1);

    field.byte_offset = json_field["byte_offset"];
    field.length = json_field["length"];
    field.scale = json_field["scale"] | 1.0f;
    field.offset = json_field["offset"] | 0.0f;

    // Parse data type
    const char* type_str = json_field["data_type"];
    field.data_type = stringToDataType(type_str);

    if (field.data_type == DataType::UNKNOWN) {
        snprintf(last_error, sizeof(last_error), "Unknown data type: %s", type_str);
        return false;
    }

    // Parse min/max
    field.has_min = json_field.containsKey("min_value");
    field.has_max = json_field.containsKey("max_value");
    if (field.has_min) field.min_value = json_field["min_value"];
    if (field.has_max) field.max_value = json_field["max_value"];

    // Parse enum values
    field.enum_count = 0;
    if (json_field.containsKey("enum_values")) {
        JsonObject enum_obj = json_field["enum_values"];
        if (!parseEnumValues(&enum_obj, field)) {
            return false;
        }
    }

    return true;
}

bool Loader::parseEnumValues(void* json_enum_ptr, Field& field) {
    JsonObject json_enum = *static_cast<JsonObject*>(json_enum_ptr);

    field.enum_count = 0;
    for (JsonPair kv : json_enum) {
        if (field.enum_count >= MAX_ENUM_VALUES) {
            setError("Too many enum values");
            return false;
        }

        field.enum_values[field.enum_count].raw_value = atoi(kv.key().c_str());
        strncpy(field.enum_values[field.enum_count].name,
                kv.value().as<const char*>(),
                sizeof(field.enum_values[field.enum_count].name) - 1);

        field.enum_count++;
    }

    return true;
}

void Loader::setError(const char* error) {
    strncpy(last_error, error, sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
}

} // namespace Protocol
