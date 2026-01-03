#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <functional>

// Forward declarations
class SettingsManager;
class BatteryManager;
class CANLogger;

// WebSocket message types
enum class WSMessageType {
    BATTERY_UPDATE,
    CAN_MESSAGE,
    SYSTEM_STATUS,
    CONFIG_CHANGED
};

// Callback for WebSocket events
using WSClientCallback = std::function<void(uint32_t client_id, bool connected)>;

class WebServer {
public:
    WebServer(uint16_t port = 80);
    ~WebServer();

    // Initialize the web server
    bool begin(SettingsManager* settings, BatteryManager* batteries, CANLogger* canLog);

    // Stop the server
    void stop();

    // WebSocket broadcasting
    void broadcastBatteryUpdate();
    void broadcastCANMessage(uint32_t id, uint8_t dlc, const uint8_t* data);
    void broadcastSystemStatus();
    void broadcastText(const char* message);

    // Get connected WebSocket client count
    uint32_t getWSClientCount() const;

    // Set callbacks
    void setClientCallback(WSClientCallback callback) { client_callback_ = callback; }

    // Statistics
    uint32_t getRequestCount() const { return request_count_; }
    uint32_t getWSMessagesSent() const { return ws_messages_sent_; }

private:
    AsyncWebServer server_;
    AsyncWebSocket ws_;
    uint16_t port_;

    SettingsManager* settings_;
    BatteryManager* batteries_;
    CANLogger* can_logger_;

    WSClientCallback client_callback_;

    uint32_t request_count_;
    uint32_t ws_messages_sent_;

    // Setup handlers
    void setupStaticFiles();
    void setupAPIEndpoints();
    void setupWebSocket();

    // API handlers
    void handleGetStatus(AsyncWebServerRequest* request);
    void handleGetBatteries(AsyncWebServerRequest* request);
    void handleGetBattery(AsyncWebServerRequest* request, uint8_t id);
    void handleGetCANLog(AsyncWebServerRequest* request);
    void handleDownloadCANLog(AsyncWebServerRequest* request);
    void handleClearCANLog(AsyncWebServerRequest* request);
    void handleGetConfig(AsyncWebServerRequest* request);
    void handlePostConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handlePostBatteryConfig(AsyncWebServerRequest* request, uint8_t id, uint8_t* data, size_t len);
    void handleCalibrate(AsyncWebServerRequest* request, uint8_t id);
    void handleReset(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);

    // WebSocket handlers
    void onWSEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                   AwsEventType type, void* arg, uint8_t* data, size_t len);

    // JSON builders
    void buildStatusJSON(JsonObject obj);
    void buildBatteryJSON(JsonObject obj, uint8_t id);
    void buildAllBatteriesJSON(JsonObject obj);
    void buildConfigJSON(JsonObject obj);
    void buildSystemJSON(JsonObject obj);

    // Utility
    void sendJSON(AsyncWebServerRequest* request, JsonDocument& doc, int code = 200);
    void sendError(AsyncWebServerRequest* request, int code, const char* message);
};

// Global instance
extern WebServer webServer;

#endif // WEB_SERVER_H
