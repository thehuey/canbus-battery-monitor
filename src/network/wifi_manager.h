#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <functional>

// WiFi connection states
enum class WiFiState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AP_MODE,
    ERROR
};

// Callback types
using WiFiStateCallback = std::function<void(WiFiState state)>;
using WiFiEventCallback = std::function<void(WiFiEvent_t event, WiFiEventInfo_t info)>;

class WiFiManager {
public:
    WiFiManager();

    // Initialize WiFi subsystem
    bool begin();

    // Connect to configured STA network
    bool connectSTA(const char* ssid, const char* password, uint32_t timeout_ms = 20000);

    // Start AP mode for configuration
    bool startAP(const char* ssid, const char* password = nullptr);

    // Start AP+STA mode (simultaneous)
    bool startAPSTA(const char* sta_ssid, const char* sta_password,
                    const char* ap_ssid, const char* ap_password = nullptr);

    // Stop WiFi
    void stop();

    // Update (call from task loop for auto-reconnect)
    void update();

    // Get current state
    WiFiState getState() const { return state_; }
    bool isConnected() const { return state_ == WiFiState::CONNECTED; }
    bool isAPActive() const { return ap_active_; }

    // Connection info
    IPAddress getLocalIP() const;
    IPAddress getAPIP() const;
    String getSSID() const;
    int8_t getRSSI() const;
    String getMACAddress() const;

    // Callbacks
    void setStateCallback(WiFiStateCallback callback) { state_callback_ = callback; }

    // Configuration
    void setAutoReconnect(bool enable) { auto_reconnect_ = enable; }
    void setReconnectDelay(uint32_t delay_ms) { reconnect_delay_ = delay_ms; }

    // Statistics
    uint32_t getConnectionAttempts() const { return connection_attempts_; }
    uint32_t getDisconnectCount() const { return disconnect_count_; }
    uint32_t getUptimeMs() const;

private:
    WiFiState state_;
    bool ap_active_;
    bool auto_reconnect_;
    uint32_t reconnect_delay_;
    uint32_t last_reconnect_attempt_;
    uint32_t connected_since_;
    uint32_t connection_attempts_;
    uint32_t disconnect_count_;

    // Stored credentials for reconnection
    String sta_ssid_;
    String sta_password_;

    WiFiStateCallback state_callback_;

    void setState(WiFiState new_state);
    void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
    static void wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info);

    static WiFiManager* instance_;
};

// Global instance
extern WiFiManager wifiManager;

#endif // WIFI_MANAGER_H
