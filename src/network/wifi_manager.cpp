#include "wifi_manager.h"
#include "../config/config.h"
#include "../utils/remote_log.h"

// Global instance
WiFiManager wifiManager;
WiFiManager* WiFiManager::instance_ = nullptr;

WiFiManager::WiFiManager()
    : state_(WiFiState::DISCONNECTED)
    , ap_active_(false)
    , auto_reconnect_(true)
    , reconnect_delay_(MQTT_RECONNECT_DELAY)
    , last_reconnect_attempt_(0)
    , connected_since_(0)
    , connection_attempts_(0)
    , disconnect_count_(0)
    , state_callback_(nullptr) {
    instance_ = this;
}

bool WiFiManager::begin() {
    LOG_INFO("[WiFi] Initializing WiFi subsystem");

    // Reduce WiFi TX power to minimize current draw during initialization
    // This helps prevent brownout on weak power supplies
    WiFi.setTxPower(WIFI_POWER_5dBm);  // Minimum power, increase later if needed
    delay(50);

    // Temporarily enable WiFi to read MAC address (needed for AP SSID generation)
    WiFi.mode(WIFI_STA);
    delay(200);  // Longer delay for power stabilization

    // Log MAC address for debugging
    LOG_INFO("[WiFi] MAC Address: %s",WiFi.macAddress().c_str());

    // Set WiFi mode to off initially
    WiFi.mode(WIFI_OFF);
    delay(100);

    // Register event handler
    WiFi.onEvent(wifiEventHandler);

    LOG_INFO("[WiFi] WiFi subsystem initialized");
    return true;
}

bool WiFiManager::connectSTA(const char* ssid, const char* password, uint32_t timeout_ms) {
    if (ssid == nullptr || strlen(ssid) == 0) {
        LOG_INFO("[WiFi] Error: No SSID provided");
        setState(WiFiState::ERROR);
        return false;
    }

    LOG_INFO("[WiFi] Connecting to '%s'...",ssid);

    // Store credentials for auto-reconnect
    sta_ssid_ = ssid;
    sta_password_ = password ? password : "";

    // Set mode and begin connection
    WiFi.mode(ap_active_ ? WIFI_AP_STA : WIFI_STA);
    WiFi.begin(ssid, password);

    setState(WiFiState::CONNECTING);
    connection_attempts_++;
    last_reconnect_attempt_ = millis();

    // Wait for connection with timeout
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
        delay(100);

        // Print progress dots
        if ((millis() - start) % 1000 < 100) {
            Serial.print(".");
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        connected_since_ = millis();
        setState(WiFiState::CONNECTED);
        LOG_INFO("[WiFi] Connected! IP: %s, RSSI: %d dBm\n",
                     WiFi.localIP().toString().c_str(), WiFi.RSSI());
        return true;
    } else {
        LOG_INFO("[WiFi] Connection failed");
        setState(WiFiState::DISCONNECTED);
        return false;
    }
}

bool WiFiManager::startAP(const char* ssid, const char* password) {
    LOG_INFO("[WiFi] Starting AP mode: %s",ssid);

    // Set low TX power before mode change to reduce current spike
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    delay(50);

    WiFi.mode(WIFI_AP);
    delay(200);  // Give WiFi time to switch modes and stabilize power

    bool success;
    if (password && strlen(password) >= 8) {
        // WiFi.softAP(ssid, password, channel, hidden, max_connections)
        success = WiFi.softAP(ssid, password, 1, false, 4);
        LOG_INFO("[WiFi] Starting secured AP on channel 1\n");
    } else {
        success = WiFi.softAP(ssid, "", 1, false, 4);
        if (password && strlen(password) > 0) {
            LOG_INFO("[WiFi] Warning: Password too short, AP is open");
        }
        LOG_INFO("[WiFi] Starting open AP on channel 1\n");
    }

    if (success) {
        ap_active_ = true;
        setState(WiFiState::AP_MODE);
        LOG_INFO("[WiFi] AP started successfully!\n");
        LOG_INFO("[WiFi] SSID: %s",ssid);
        LOG_INFO("[WiFi] Password: %s",(password && strlen(password) >= 8) ? password : "(open)");
        LOG_INFO("[WiFi] IP: %s",WiFi.softAPIP().toString().c_str());
        LOG_INFO("[WiFi] MAC: %s",WiFi.softAPmacAddress().c_str());
        return true;
    } else {
        LOG_INFO("[WiFi] Failed to start AP");
        setState(WiFiState::ERROR);
        return false;
    }
}

bool WiFiManager::startAPSTA(const char* sta_ssid, const char* sta_password,
                             const char* ap_ssid, const char* ap_password) {
    LOG_INFO("[WiFi] Starting AP+STA mode");

    // Set low TX power before mode change to reduce current spike
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    delay(50);

    // Start AP first
    WiFi.mode(WIFI_AP_STA);
    delay(200);  // Give WiFi time to switch modes and stabilize power

    bool ap_success;
    if (ap_password && strlen(ap_password) >= 8) {
        // WiFi.softAP(ssid, password, channel, hidden, max_connections)
        ap_success = WiFi.softAP(ap_ssid, ap_password, 1, false, 4);
        LOG_INFO("[WiFi] Starting secured AP on channel 1\n");
    } else {
        ap_success = WiFi.softAP(ap_ssid, "", 1, false, 4);
        LOG_INFO("[WiFi] Starting open AP on channel 1\n");
    }

    if (ap_success) {
        ap_active_ = true;
        LOG_INFO("[WiFi] AP started successfully!\n");
        LOG_INFO("[WiFi] AP SSID: %s",ap_ssid);
        LOG_INFO("[WiFi] AP Password: %s",(ap_password && strlen(ap_password) >= 8) ? ap_password : "(open)");
        LOG_INFO("[WiFi] AP IP: %s",WiFi.softAPIP().toString().c_str());
        LOG_INFO("[WiFi] AP MAC: %s",WiFi.softAPmacAddress().c_str());
    } else {
        LOG_INFO("[WiFi] Warning: Failed to start AP");
    }

    // Then connect to STA
    if (sta_ssid && strlen(sta_ssid) > 0) {
        sta_ssid_ = sta_ssid;
        sta_password_ = sta_password ? sta_password : "";

        LOG_INFO("[WiFi] Attempting STA connection to: %s",sta_ssid);
        WiFi.begin(sta_ssid, sta_password);
        setState(WiFiState::CONNECTING);
        connection_attempts_++;
        last_reconnect_attempt_ = millis();
    } else {
        setState(WiFiState::AP_MODE);
    }

    return ap_success;
}

void WiFiManager::stop() {
    LOG_INFO("[WiFi] Stopping WiFi");
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    ap_active_ = false;
    setState(WiFiState::DISCONNECTED);
}

void WiFiManager::update() {
    // Check if STA connection was lost and auto-reconnect is enabled
    if (auto_reconnect_ &&
        state_ == WiFiState::DISCONNECTED &&
        sta_ssid_.length() > 0 &&
        (millis() - last_reconnect_attempt_) > reconnect_delay_) {

        LOG_INFO("[WiFi] Attempting auto-reconnect...");
        connection_attempts_++;
        last_reconnect_attempt_ = millis();

        WiFi.begin(sta_ssid_.c_str(), sta_password_.c_str());
        setState(WiFiState::CONNECTING);
    }

    // Check if connection completed (for non-blocking reconnect)
    if (state_ == WiFiState::CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            connected_since_ = millis();
            setState(WiFiState::CONNECTED);
            LOG_INFO("[WiFi] Reconnected! IP: %s",WiFi.localIP().toString().c_str());
        } else if ((millis() - last_reconnect_attempt_) > WIFI_CONNECTION_TIMEOUT) {
            LOG_INFO("[WiFi] Connection attempt timed out");
            setState(WiFiState::DISCONNECTED);
        }
    }
}

IPAddress WiFiManager::getLocalIP() const {
    return WiFi.localIP();
}

IPAddress WiFiManager::getAPIP() const {
    return WiFi.softAPIP();
}

String WiFiManager::getSSID() const {
    return WiFi.SSID();
}

int8_t WiFiManager::getRSSI() const {
    return WiFi.RSSI();
}

String WiFiManager::getMACAddress() const {
    return WiFi.macAddress();
}

uint32_t WiFiManager::getUptimeMs() const {
    if (state_ == WiFiState::CONNECTED && connected_since_ > 0) {
        return millis() - connected_since_;
    }
    return 0;
}

void WiFiManager::setState(WiFiState new_state) {
    if (state_ != new_state) {
        state_ = new_state;

        // Log state change
        const char* state_names[] = {
            "DISCONNECTED", "CONNECTING", "CONNECTED", "AP_MODE", "ERROR"
        };
        LOG_INFO("[WiFi] State changed to: %s",state_names[static_cast<int>(new_state)]);

        // Invoke callback if set
        if (state_callback_) {
            state_callback_(new_state);
        }
    }
}

void WiFiManager::wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (instance_ == nullptr) return;
    instance_->onWiFiEvent(event, info);
}

void WiFiManager::onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            LOG_INFO("[WiFi] Got IP: %s",WiFi.localIP().toString().c_str());
            connected_since_ = millis();
            setState(WiFiState::CONNECTED);
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            LOG_INFO("[WiFi] Disconnected (reason: %d)",info.wifi_sta_disconnected.reason);
            disconnect_count_++;
            if (ap_active_) {
                setState(WiFiState::AP_MODE);
            } else {
                setState(WiFiState::DISCONNECTED);
            }
            break;

        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            LOG_INFO("[WiFi] Station connected to AP");
            break;

        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            LOG_INFO("[WiFi] Client connected to AP, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                         info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
                         info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
                         info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
            break;

        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            LOG_INFO("[WiFi] Client disconnected from AP");
            break;

        default:
            break;
    }
}
