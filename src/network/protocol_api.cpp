#include "web_server.h"
#include "../can/protocol_loader.h"
#include "../can/builtin_protocols.h"
#include <SPIFFS.h>

// Protocol API Endpoint Implementations

void WebServer::handleGetProtocols(AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    // Built-in protocols
    JsonArray builtin = root["builtin"].to<JsonArray>();
    uint8_t builtin_count;
    const Protocol::Definition* const* builtin_protocols = Protocol::getAllBuiltinProtocols(builtin_count);

    for (uint8_t i = 0; i < builtin_count; i++) {
        JsonObject proto = builtin.add<JsonObject>();
        proto["id"] = i;
        proto["name"] = builtin_protocols[i]->name;
        proto["manufacturer"] = builtin_protocols[i]->manufacturer;
        proto["version"] = builtin_protocols[i]->version;
        proto["source"] = "builtin";
    }

    // Custom protocols
    JsonArray custom = root["custom"].to<JsonArray>();

    if (protocol_loader_) {
        Protocol::Loader::ProtocolInfo protocols[10];
        uint8_t count = protocol_loader_->listCustomProtocols(protocols, 10);

        for (uint8_t i = 0; i < count; i++) {
            JsonObject proto = custom.add<JsonObject>();
            proto["id"] = i;
            proto["filename"] = protocols[i].filename;
            proto["name"] = protocols[i].name;
            proto["manufacturer"] = protocols[i].manufacturer;
            proto["size"] = protocols[i].file_size;
            proto["source"] = "custom";
        }
    }

    sendJSON(request, doc);
}

void WebServer::handleGetBuiltinProtocols(AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    uint8_t builtin_count;
    const Protocol::Definition* const* builtin_protocols = Protocol::getAllBuiltinProtocols(builtin_count);

    for (uint8_t i = 0; i < builtin_count; i++) {
        JsonObject proto = array.add<JsonObject>();
        proto["id"] = i;
        proto["name"] = builtin_protocols[i]->name;
        proto["manufacturer"] = builtin_protocols[i]->manufacturer;
        proto["version"] = builtin_protocols[i]->version;
        proto["description"] = builtin_protocols[i]->description;
        proto["cell_count"] = builtin_protocols[i]->cell_count;
        proto["nominal_voltage"] = builtin_protocols[i]->nominal_voltage;
        proto["chemistry"] = builtin_protocols[i]->chemistry;
        proto["message_count"] = builtin_protocols[i]->message_count;
    }

    sendJSON(request, doc);
}

void WebServer::handleGetCustomProtocols(AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    if (protocol_loader_) {
        Protocol::Loader::ProtocolInfo protocols[10];
        uint8_t count = protocol_loader_->listCustomProtocols(protocols, 10);

        for (uint8_t i = 0; i < count; i++) {
            JsonObject proto = array.add<JsonObject>();
            proto["id"] = i;
            proto["filename"] = protocols[i].filename;
            proto["name"] = protocols[i].name;
            proto["manufacturer"] = protocols[i].manufacturer;
            proto["size"] = protocols[i].file_size;
        }
    }

    sendJSON(request, doc);
}

void WebServer::handleGetProtocol(AsyncWebServerRequest* request, const String& id) {
    if (!protocol_loader_) {
        sendError(request, 503, "Protocol loader not available");
        return;
    }

    // Check if it's a built-in protocol (id starts with "builtin_")
    if (id.startsWith("builtin_")) {
        int builtin_id = id.substring(8).toInt();
        const Protocol::Definition* proto = Protocol::getBuiltinProtocol(
            static_cast<Protocol::BuiltinId>(builtin_id));

        if (!proto) {
            sendError(request, 404, "Built-in protocol not found");
            return;
        }

        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();

        root["name"] = proto->name;
        root["manufacturer"] = proto->manufacturer;
        root["version"] = proto->version;
        root["description"] = proto->description;
        root["cell_count"] = proto->cell_count;
        root["nominal_voltage"] = proto->nominal_voltage;
        root["capacity_ah"] = proto->capacity_ah;
        root["chemistry"] = proto->chemistry;

        JsonArray messages = root["messages"].to<JsonArray>();
        for (uint8_t i = 0; i < proto->message_count; i++) {
            JsonObject msg = messages.add<JsonObject>();
            msg["can_id"] = proto->messages[i].can_id;
            msg["name"] = proto->messages[i].name;
            msg["description"] = proto->messages[i].description;
            msg["period_ms"] = proto->messages[i].period_ms;
            msg["field_count"] = proto->messages[i].field_count;
        }

        sendJSON(request, doc);
        return;
    }

    // Otherwise, it's a custom protocol file
    String filepath = "/protocols/" + id;
    if (!filepath.endsWith(".json")) {
        filepath += ".json";
    }

    if (!SPIFFS.exists(filepath)) {
        sendError(request, 404, "Protocol file not found");
        return;
    }

    // Read and return the file
    File file = SPIFFS.open(filepath, "r");
    if (!file) {
        sendError(request, 500, "Failed to open protocol file");
        return;
    }

    request->send(file, filepath, "application/json");
    file.close();
}

void WebServer::handleUploadProtocol(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!protocol_loader_) {
        sendError(request, 503, "Protocol loader not available");
        return;
    }

    // Parse the JSON to get the protocol name
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        sendError(request, 400, "Invalid JSON");
        return;
    }

    const char* name = doc["name"];
    if (!name || strlen(name) == 0) {
        sendError(request, 400, "Protocol name required");
        return;
    }

    // Validate the protocol
    Protocol::Definition protocol;
    if (!protocol_loader_->loadFromString((const char*)data, protocol)) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Protocol validation failed: %s",
                protocol_loader_->getLastError());
        sendError(request, 400, error_msg);
        return;
    }

    // Generate a filename
    char filename[48];
    uint8_t custom_id = 0;

    // Find the next available custom_X.json filename
    while (custom_id < 100) {
        snprintf(filename, sizeof(filename), "/protocols/custom_%d.json", custom_id);
        if (!SPIFFS.exists(filename)) {
            break;
        }
        custom_id++;
    }

    if (custom_id >= 100) {
        sendError(request, 507, "Too many custom protocols");
        return;
    }

    // Save the protocol
    if (!protocol_loader_->saveToFile(filename, protocol)) {
        sendError(request, 500, "Failed to save protocol");
        return;
    }

    // Return success with the filename
    JsonDocument response;
    response["success"] = true;
    response["filename"] = filename;
    response["name"] = protocol.name;

    sendJSON(request, response, 201);
}

void WebServer::handleFetchProtocol(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!protocol_loader_) {
        sendError(request, 503, "Protocol loader not available");
        return;
    }

    // Parse the request body to get the URL
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        sendError(request, 400, "Invalid JSON");
        return;
    }

    const char* url = doc["url"];
    if (!url || strlen(url) == 0) {
        sendError(request, 400, "URL required");
        return;
    }

    // Generate a filename for the fetched protocol
    char filename[48];
    uint8_t custom_id = 0;

    while (custom_id < 100) {
        snprintf(filename, sizeof(filename), "/protocols/custom_%d.json", custom_id);
        if (!SPIFFS.exists(filename)) {
            break;
        }
        custom_id++;
    }

    if (custom_id >= 100) {
        sendError(request, 507, "Too many custom protocols");
        return;
    }

    // Fetch the protocol from URL
    if (!protocol_loader_->fetchFromUrl(url, filename)) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to fetch protocol: %s",
                protocol_loader_->getLastError());
        sendError(request, 500, error_msg);
        return;
    }

    // Load it to get the name
    Protocol::Definition protocol;
    if (!protocol_loader_->loadFromFile(filename, protocol)) {
        sendError(request, 500, "Failed to load fetched protocol");
        return;
    }

    // Return success
    JsonDocument response;
    response["success"] = true;
    response["filename"] = filename;
    response["name"] = protocol.name;
    response["source_url"] = url;

    sendJSON(request, response, 201);
}

void WebServer::handleDeleteProtocol(AsyncWebServerRequest* request, const String& id) {
    if (!protocol_loader_) {
        sendError(request, 503, "Protocol loader not available");
        return;
    }

    String filepath = "/protocols/" + id;
    if (!filepath.endsWith(".json")) {
        filepath += ".json";
    }

    if (!protocol_loader_->deleteProtocol(filepath.c_str())) {
        sendError(request, 404, "Protocol not found or cannot be deleted");
        return;
    }

    JsonDocument response;
    response["success"] = true;
    response["message"] = "Protocol deleted";

    sendJSON(request, response);
}

void WebServer::handleValidateProtocol(AsyncWebServerRequest* request, const String& id) {
    if (!protocol_loader_) {
        sendError(request, 503, "Protocol loader not available");
        return;
    }

    String filepath = "/protocols/" + id;
    if (!filepath.endsWith(".json")) {
        filepath += ".json";
    }

    Protocol::Definition protocol;
    if (!protocol_loader_->loadFromFile(filepath.c_str(), protocol)) {
        JsonDocument response;
        response["valid"] = false;
        response["error"] = protocol_loader_->getLastError();
        sendJSON(request, response);
        return;
    }

    if (!protocol_loader_->validate(protocol)) {
        JsonDocument response;
        response["valid"] = false;
        response["error"] = "Protocol validation failed";
        sendJSON(request, response);
        return;
    }

    JsonDocument response;
    response["valid"] = true;
    response["name"] = protocol.name;
    response["message_count"] = protocol.message_count;

    sendJSON(request, response);
}
