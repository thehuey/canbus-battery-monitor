#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "../config/config.h"

// Forward declarations
class SettingsManager;
class BatteryManager;
struct CANMessage;

// MQTT connection state
enum class MQTTState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

class MQTTClient {
public:
    MQTTClient();
    ~MQTTClient();

    // Initialize MQTT client with settings
    bool begin(SettingsManager* settings, BatteryManager* batteries);

    // Update loop - handles reconnection and keep-alive
    void update();

    // Connection management
    bool connect();
    void disconnect();
    bool isConnected();  // Cannot be const due to PubSubClient::connected()
    MQTTState getState() const { return state_; }

    // Publishing methods
    bool publishBatteryStatus(uint8_t battery_id);
    bool publishAllBatteries();
    bool publishSystemStatus();
    bool publishCANRaw(uint32_t can_id, uint8_t dlc, const uint8_t* data);
    bool publishCANMessage(const CANMessage& msg);  // New: Publish CAN message to canmsg topic
    bool publishConfig();

    // Generic publish
    bool publish(const char* topic, const char* payload, bool retained = false);

    // Statistics
    uint32_t getPublishCount() const { return publish_count_; }
    uint32_t getFailedPublishCount() const { return failed_publish_count_; }
    uint32_t getReconnectCount() const { return reconnect_count_; }
    const char* getLastError() const { return last_error_; }

    // Enable/disable MQTT
    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled_; }

private:
    WiFiClientSecure wifi_client_;
    PubSubClient mqtt_client_;

    SettingsManager* settings_;
    BatteryManager* batteries_;

    MQTTState state_;
    bool enabled_;

    uint32_t last_connect_attempt_;
    uint32_t reconnect_delay_;
    uint32_t reconnect_count_;

    uint32_t publish_count_;
    uint32_t failed_publish_count_;

    char last_error_[128];

    // Topic building
    void buildTopic(char* buffer, size_t buflen, const char* subtopic);

    // TLS certificate setup
    void setupTLS();

    // Connection helpers
    bool connectToBroker();
    void handleConnectionError();

    // Callbacks (for future subscription support)
    static void messageCallback(char* topic, uint8_t* payload, unsigned int length);

    void setError(const char* error);
};

// Global instance
extern MQTTClient mqttClient;

#endif // MQTT_CLIENT_H
