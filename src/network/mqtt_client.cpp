#include "mqtt_client.h"
#include "../config/settings.h"
#include "../battery/battery_manager.h"
#include "../can/can_message.h"
#include "../utils/remote_log.h"
#include <ArduinoJson.h>

#ifndef MQTT_DISABLE_TLS
// HiveMQ Cloud root CA certificate
// This is the ISRG Root X1 certificate used by HiveMQ Cloud (Let's Encrypt)
static const char* HIVEMQ_ROOT_CA = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";
#endif

// Global instance
MQTTClient mqttClient;

MQTTClient::MQTTClient()
    : mqtt_client_(wifi_client_),
      settings_(nullptr),
      batteries_(nullptr),
      state_(MQTTState::DISCONNECTED),
      enabled_(true),
      last_connect_attempt_(0),
      reconnect_delay_(MQTT_RECONNECT_DELAY),
      reconnect_count_(0),
      publish_count_(0),
      failed_publish_count_(0) {
    last_error_[0] = '\0';
}

MQTTClient::~MQTTClient() {
    disconnect();
}

bool MQTTClient::begin(SettingsManager* settings, BatteryManager* batteries) {
    settings_ = settings;
    batteries_ = batteries;

    if (!settings_ || !batteries_) {
        setError("Invalid parameters");
        return false;
    }

    const Settings& config = settings_->getSettings();

    // Check if MQTT is enabled
    if (!config.mqtt_enabled) {
        LOG_INFO("[MQTT] MQTT disabled in settings");
        enabled_ = false;
        return true;  // Not an error, just disabled
    }

    // Check if MQTT is configured
    if (strlen(config.mqtt_broker) == 0) {
        LOG_INFO("[MQTT] No broker configured, MQTT disabled");
        enabled_ = false;
        return true;  // Not an error, just not configured
    }

    LOG_INFO("[MQTT] Initializing MQTT client...");
    LOG_INFO("[MQTT] Broker: %s:%d",config.mqtt_broker, config.mqtt_port);
    LOG_INFO("[MQTT] Topic prefix: %s",config.mqtt_topic_prefix);

    // Setup TLS
    setupTLS();

    // Configure MQTT client
    mqtt_client_.setServer(config.mqtt_broker, config.mqtt_port);
    mqtt_client_.setCallback(messageCallback);
    mqtt_client_.setBufferSize(512);  // Increase buffer for larger payloads
    mqtt_client_.setKeepAlive(60);    // 60 second keep-alive

    LOG_INFO("[MQTT] MQTT client initialized");
    return true;
}

void MQTTClient::setupTLS() {
#ifndef MQTT_DISABLE_TLS
    // Set root CA certificate for HiveMQ Cloud
    wifi_client_.setCACert(HIVEMQ_ROOT_CA);
#else
    // TLS disabled - use insecure connection
    wifi_client_.setInsecure();
#endif

    // Set timeout
    wifi_client_.setTimeout(10);  // 10 second timeout

    LOG_INFO("[MQTT] TLS configured with root CA certificate");
}

void MQTTClient::update() {
    if (!enabled_ || !settings_) {
        return;
    }

    // Handle reconnection
    if (!mqtt_client_.connected()) {
        if (state_ == MQTTState::CONNECTED) {
            LOG_INFO("[MQTT] Connection lost");
            state_ = MQTTState::DISCONNECTED;
        }

        // Attempt reconnect if enough time has passed
        uint32_t now = millis();
        if (now - last_connect_attempt_ > reconnect_delay_) {
            if (WiFi.status() == WL_CONNECTED) {
                LOG_INFO("[MQTT] Attempting to connect...");
                connect();
            }
            last_connect_attempt_ = now;
        }
    } else {
        // Keep connection alive
        mqtt_client_.loop();

        if (state_ != MQTTState::CONNECTED) {
            state_ = MQTTState::CONNECTED;
        }
    }
}

bool MQTTClient::connect() {
    if (!enabled_ || !settings_) {
        return false;
    }

    const Settings& config = settings_->getSettings();

    if (strlen(config.mqtt_broker) == 0) {
        setError("No broker configured");
        return false;
    }

    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        setError("WiFi not connected");
        return false;
    }

    state_ = MQTTState::CONNECTING;

    // Generate client ID from MAC address
    String client_id = "ebike-" + WiFi.macAddress();
    client_id.replace(":", "");

    LOG_INFO("[MQTT] Connecting to %s:%d as %s\n",
                 config.mqtt_broker, config.mqtt_port, client_id.c_str());

    bool connected = false;

    // Connect with username/password if provided
    if (strlen(config.mqtt_username) > 0) {
        LOG_INFO("[MQTT] Using authentication (username: %s)",config.mqtt_username);
        connected = mqtt_client_.connect(
            client_id.c_str(),
            config.mqtt_username,
            config.mqtt_password
        );
    } else {
        LOG_INFO("[MQTT] Connecting without authentication");
        connected = mqtt_client_.connect(client_id.c_str());
    }

    if (connected) {
        state_ = MQTTState::CONNECTED;
        reconnect_count_++;
        reconnect_delay_ = MQTT_RECONNECT_DELAY;  // Reset delay

        LOG_INFO("[MQTT] Connected successfully!");

        // Publish initial config
        publishConfig();

        return true;
    } else {
        handleConnectionError();
        return false;
    }
}

void MQTTClient::handleConnectionError() {
    state_ = MQTTState::ERROR;

    int error_code = mqtt_client_.state();
    const char* error_msg = "Unknown error";

    switch (error_code) {
        case -4: error_msg = "Connection timeout"; break;
        case -3: error_msg = "Connection lost"; break;
        case -2: error_msg = "Connect failed"; break;
        case -1: error_msg = "Disconnected"; break;
        case 1: error_msg = "Bad protocol"; break;
        case 2: error_msg = "Bad client ID"; break;
        case 3: error_msg = "Unavailable"; break;
        case 4: error_msg = "Bad credentials"; break;
        case 5: error_msg = "Unauthorized"; break;
    }

    LOG_WARN("[MQTT] Connection failed: %s (code: %d)", error_msg, error_code);
    setError(error_msg);

    // Exponential backoff (max 60 seconds)
    reconnect_delay_ = min(reconnect_delay_ * 2, (uint32_t)60000);
    LOG_INFO("[MQTT] Will retry in %u seconds",reconnect_delay_ / 1000);
}

void MQTTClient::disconnect() {
    if (mqtt_client_.connected()) {
        mqtt_client_.disconnect();
        LOG_INFO("[MQTT] Disconnected");
    }
    state_ = MQTTState::DISCONNECTED;
}

bool MQTTClient::isConnected() {
    return mqtt_client_.connected() && state_ == MQTTState::CONNECTED;
}

bool MQTTClient::publishBatteryStatus(uint8_t battery_id) {
    if (!isConnected() || !batteries_) {
        return false;
    }

    const BatteryModule* battery = batteries_->getBattery(battery_id);
    if (!battery || !battery->isEnabled()) {
        return false;
    }

    // Build JSON payload
    JsonDocument doc;
    doc["id"] = battery_id;
    doc["name"] = battery->getName();
    doc["voltage"] = battery->getVoltage();
    doc["current"] = battery->getCurrent();
    doc["power"] = battery->getPower();
    doc["soc"] = battery->getSOC();
    doc["temp1"] = battery->getTemp1();
    doc["temp2"] = battery->getTemp2();
    doc["enabled"] = battery->isEnabled();
    doc["has_can_data"] = battery->hasCANData();
    doc["data_fresh"] = battery->isDataFresh(10000);
    doc["timestamp"] = millis() / 1000;

    String payload;
    serializeJson(doc, payload);

    // Build topic: ebike/battery/0/status
    char topic[64];
    const Settings& config = settings_->getSettings();
    snprintf(topic, sizeof(topic), "%s/battery/%d/status",
             config.mqtt_topic_prefix, battery_id);

    return publish(topic, payload.c_str(), false);
}

bool MQTTClient::publishAllBatteries() {
    if (!isConnected() || !batteries_) {
        return false;
    }

    // Build JSON payload with all batteries
    JsonDocument doc;
    JsonArray batteries = doc["batteries"].to<JsonArray>();

    for (uint8_t i = 0; i < batteries_->getActiveBatteryCount(); i++) {
        const BatteryModule* battery = batteries_->getBattery(i);
        if (battery && battery->isEnabled()) {
            JsonObject bat = batteries.add<JsonObject>();
            bat["id"] = i;
            bat["name"] = battery->getName();
            bat["voltage"] = battery->getVoltage();
            bat["current"] = battery->getCurrent();
            bat["power"] = battery->getPower();
            bat["soc"] = battery->getSOC();
        }
    }

    doc["total_power"] = batteries_->getTotalPower();
    doc["total_current"] = batteries_->getTotalCurrent();
    doc["avg_voltage"] = batteries_->getAverageVoltage();
    doc["timestamp"] = millis() / 1000;

    String payload;
    serializeJson(doc, payload);

    // Build topic: ebike/battery/all/status
    char topic[64];
    const Settings& config = settings_->getSettings();
    snprintf(topic, sizeof(topic), "%s/battery/all/status",
             config.mqtt_topic_prefix);

    return publish(topic, payload.c_str(), false);
}

bool MQTTClient::publishSystemStatus() {
    if (!isConnected()) {
        return false;
    }

    JsonDocument doc;
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["wifi_ssid"] = WiFi.SSID();
    doc["ip_address"] = WiFi.localIP().toString();
    doc["mqtt_publishes"] = publish_count_;
    doc["mqtt_failures"] = failed_publish_count_;
    doc["timestamp"] = millis() / 1000;

    String payload;
    serializeJson(doc, payload);

    char topic[64];
    const Settings& config = settings_->getSettings();
    snprintf(topic, sizeof(topic), "%s/system/status",
             config.mqtt_topic_prefix);

    return publish(topic, payload.c_str(), false);
}

bool MQTTClient::publishCANRaw(uint32_t can_id, uint8_t dlc, const uint8_t* data) {
    if (!isConnected()) {
        return false;
    }

    JsonDocument doc;
    doc["id"] = String("0x") + String(can_id, HEX);
    doc["dlc"] = dlc;

    // Convert data to hex string
    String data_hex;
    for (uint8_t i = 0; i < dlc; i++) {
        char byte_hex[3];
        sprintf(byte_hex, "%02X", data[i]);
        data_hex += byte_hex;
    }
    doc["data"] = data_hex;
    doc["timestamp"] = millis();

    String payload;
    serializeJson(doc, payload);

    char topic[64];
    const Settings& config = settings_->getSettings();
    snprintf(topic, sizeof(topic), "%s/can/raw",
             config.mqtt_topic_prefix);

    return publish(topic, payload.c_str(), false);
}

bool MQTTClient::publishCANMessage(const CANMessage& msg) {
    if (!isConnected()) {
        return false;
    }

    // Check if CAN message publishing is enabled
    const Settings& config = settings_->getSettings();
    if (!config.mqtt_canmsg_enabled) {
        return false;  // Silently skip if disabled
    }

    JsonDocument doc;

    // Format CAN ID as hex string
    char id_str[12];
    snprintf(id_str, sizeof(id_str), "0x%03X", msg.id);
    doc["id"] = id_str;
    doc["dlc"] = msg.dlc;
    doc["extended"] = msg.extended;
    doc["rtr"] = msg.rtr;
    doc["timestamp"] = msg.timestamp;

    // Convert data bytes to hex string array for readability
    JsonArray data_array = doc["data"].to<JsonArray>();
    for (uint8_t i = 0; i < msg.dlc && i < 8; i++) {
        char byte_str[4];
        snprintf(byte_str, sizeof(byte_str), "%02X", msg.data[i]);
        data_array.add(byte_str);
    }

    String payload;
    serializeJson(doc, payload);

    // Build topic: <prefix>/canmsg
    char topic[64];
    snprintf(topic, sizeof(topic), "%s/canmsg", config.mqtt_topic_prefix);
    return publish(topic, payload.c_str(), false);
}

bool MQTTClient::publishConfig() {
    if (!isConnected() || !settings_) {
        return false;
    }

    const Settings& config = settings_->getSettings();

    JsonDocument doc;
    doc["num_batteries"] = config.num_batteries;
    doc["can_bitrate"] = config.can_bitrate;
    doc["sample_interval_ms"] = config.sample_interval_ms;
    doc["publish_interval_ms"] = config.publish_interval_ms;

    JsonArray bats = doc["batteries"].to<JsonArray>();
    for (uint8_t i = 0; i < config.num_batteries; i++) {
        JsonObject bat = bats.add<JsonObject>();
        bat["id"] = i;
        bat["name"] = config.batteries[i].name;
        bat["enabled"] = config.batteries[i].enabled;
    }

    String payload;
    serializeJson(doc, payload);

    char topic[64];
    snprintf(topic, sizeof(topic), "%s/system/config",
             config.mqtt_topic_prefix);

    // Publish with retained flag so new subscribers get the config
    return publish(topic, payload.c_str(), true);
}

bool MQTTClient::publish(const char* topic, const char* payload, bool retained) {
    if (!isConnected()) {
        failed_publish_count_++;
        return false;
    }

    bool success = mqtt_client_.publish(topic, payload, retained);

    if (success) {
        publish_count_++;
        LOG_INFO("[MQTT] Published to %s (%d bytes)",topic, strlen(payload));
    } else {
        failed_publish_count_++;
        LOG_WARN("[MQTT] Publish failed to %s", topic);
    }

    return success;
}

void MQTTClient::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled && mqtt_client_.connected()) {
        disconnect();
    }
}

void MQTTClient::messageCallback(char* topic, uint8_t* payload, unsigned int length) {
    // Callback for incoming MQTT messages (subscription support)
    // Currently not used, but available for future expansion
    LOG_INFO("[MQTT] Message received on topic: %s",topic);
}

void MQTTClient::setError(const char* error) {
    strncpy(last_error_, error, sizeof(last_error_) - 1);
    last_error_[sizeof(last_error_) - 1] = '\0';
}
