#include <Arduino.h>
#include "config/config.h"
#include "config/settings.h"
#include "battery/battery_manager.h"
#include "can/can_driver.h"
#include "can/can_parser.h"
#include "can/can_logger.h"
#include "network/wifi_manager.h"
#include "network/web_server.h"
#include "utils/remote_log.h"

// Global objects
SettingsManager settingsManager;
BatteryManager batteryManager;
CANParser canParser;

// Task handles
TaskHandle_t canTaskHandle = NULL;
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t networkTaskHandle = NULL;

// Forward declarations
void setupPins();
void setupSerial();
void setupCANBus();
void setupSensors();
void setupNetwork();
void setupWebServer();
void canTask(void* parameter);
void sensorTask(void* parameter);
void networkTask(void* parameter);

void setup() {
    // Initialize serial for debugging
    setupSerial();

    // Initialize remote logger early
    remoteLog.begin();

    LOG_INFO("=================================");
    LOG_INFO("eBike Battery CANBUS Monitor");
    LOG_INFO("=================================");

    // Configure GPIO pins
    setupPins();

    // Load settings from NVS
    LOG_INFO("Loading settings...");
    if (!settingsManager.begin()) {
        LOG_WARN("Using default settings");
    }

    // Print current settings
    settingsManager.printSettings();

    // Initialize battery manager
    const Settings& settings = settingsManager.getSettings();
    LOG_INFO("Initializing %d battery module(s)...", settings.num_batteries);
    batteryManager.begin(settings.num_batteries);

    // Configure battery modules from settings
    for (uint8_t i = 0; i < settings.num_batteries && i < MAX_BATTERY_MODULES; i++) {
        BatteryModule* battery = batteryManager.getBattery(i);
        if (battery != nullptr) {
            battery->setEnabled(settings.batteries[i].enabled);
            battery->setName(settings.batteries[i].name);
        }
    }

    // Initialize subsystems
    setupCANBus();
    setupSensors();
    setupNetwork();
    setupWebServer();

    // Create FreeRTOS tasks
    LOG_INFO("Starting tasks...");

    xTaskCreatePinnedToCore(
        canTask,            // Task function
        "CAN Task",         // Task name
        4096,               // Stack size
        NULL,               // Parameters
        2,                  // Priority
        &canTaskHandle,     // Task handle
        0                   // Core (0 or 1)
    );

    xTaskCreatePinnedToCore(
        sensorTask,
        "Sensor Task",
        4096,
        NULL,
        1,
        &sensorTaskHandle,
        0
    );

    xTaskCreatePinnedToCore(
        networkTask,
        "Network Task",
        8192,
        NULL,
        1,
        &networkTaskHandle,
        1                   // Run on core 1 (WiFi core)
    );

    LOG_INFO("System initialized successfully!");
    LOG_INFO("Type 'help' for available commands");
}

void loop() {
    // Main loop runs on core 1
    // Most work is done in FreeRTOS tasks

    // Check for serial commands
    static String serialCommand = "";
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialCommand.length() > 0) {
                serialCommand.trim();

                if (serialCommand == "reset_wifi" || serialCommand == "clear_wifi") {
                    Serial.println("\n=== Clearing WiFi Configuration ===");
                    settingsManager.clearNVS();
                    Serial.println("WiFi settings cleared. Rebooting in 2 seconds...");
                    delay(2000);
                    ESP.restart();
                } else if (serialCommand == "help") {
                    Serial.println("\n=== Available Commands ===");
                    Serial.println("  reset_wifi / clear_wifi - Clear WiFi credentials and reboot");
                    Serial.println("  help - Show this help message");
                    Serial.println("==========================\n");
                } else {
                    Serial.printf("Unknown command: %s (type 'help' for available commands)\n",
                                 serialCommand.c_str());
                }

                serialCommand = "";
            }
        } else {
            serialCommand += c;
        }
    }

    // Update battery manager
    batteryManager.update();

    // Monitor battery health
    static uint32_t lastHealthCheck = 0;
    if (millis() - lastHealthCheck > 30000) {  // Every 30 seconds
        if (!batteryManager.allBatteriesHealthy()) {
            uint8_t errorCount = batteryManager.getErrorCount();
            LOG_WARN("%d battery error(s) detected", errorCount);

            // Print detailed status
            for (uint8_t i = 0; i < batteryManager.getActiveBatteryCount(); i++) {
                const BatteryModule* battery = batteryManager.getBattery(i);
                if (battery != nullptr && battery->isEnabled()) {
                    if (battery->hasError() || !battery->isDataFresh(10000)) {
                        if (!battery->isDataFresh(10000)) {
                            LOG_WARN("Battery %d (%s): STALE DATA", i, battery->getName());
                        } else if (battery->hasError()) {
                            LOG_ERROR("Battery %d (%s): ERROR FLAG SET", i, battery->getName());
                        }
                    }
                }
            }
        }
        lastHealthCheck = millis();
    }

    // Monitor free heap
    static uint32_t lastHeapCheck = 0;
    if (millis() - lastHeapCheck > 10000) {  // Every 10 seconds
        uint32_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < HEAP_WARNING_THRESHOLD) {
            LOG_WARN("Low heap memory: %u bytes", freeHeap);
        }
        lastHeapCheck = millis();
    }

    // Print battery summary periodically
    static uint32_t lastSummary = 0;
    if (millis() - lastSummary > 60000) {  // Every 60 seconds
        Serial.println("\n========== Battery Summary ==========");
        for (uint8_t i = 0; i < batteryManager.getActiveBatteryCount(); i++) {
            const BatteryModule* battery = batteryManager.getBattery(i);
            if (battery != nullptr && battery->isEnabled()) {
                Serial.printf("Battery %d (%s):\n", i, battery->getName());
                Serial.printf("  Voltage: %.2f V\n", battery->getVoltage());
                Serial.printf("  Current: %.2f A\n", battery->getCurrent());
                Serial.printf("  Power: %.2f W\n", battery->getPower());
                Serial.printf("  SOC: %d%%\n", battery->getSOC());
                Serial.printf("  Temp1: %.1f°C, Temp2: %.1f°C\n",
                             battery->getTemp1(), battery->getTemp2());
                Serial.printf("  Data age: %lu ms\n",
                             millis() - battery->getLastUpdate());
                Serial.printf("  Has CAN data: %s\n",
                             battery->hasCANData() ? "Yes" : "No");
            }
        }
        Serial.printf("Total Power: %.2f W\n", batteryManager.getTotalPower());
        Serial.printf("Total Current: %.2f A\n", batteryManager.getTotalCurrent());
        Serial.printf("Average Voltage: %.2f V\n", batteryManager.getAverageVoltage());
        Serial.println("=====================================\n");
        lastSummary = millis();
    }

    delay(100);  // Small delay to prevent watchdog issues
}

// Setup functions
void setupSerial() {
    Serial.begin(115200);
    delay(1000);  // Wait for serial to be ready
}

void setupPins() {
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);

    // ADC pins are input by default, no need to configure
    LOG_INFO("GPIO pins configured");
}

void setupCANBus() {
    LOG_INFO("Initializing CAN bus...");

    // Initialize CAN logger
    if (!canLogger.begin("/canlog.csv")) {
        LOG_WARN("CAN logger initialization failed");
    }

    // Initialize CAN driver
    uint32_t bitrate = settingsManager.getSettings().can_bitrate;
    if (!canDriver.begin(bitrate)) {
        LOG_ERROR("CAN driver initialization failed!");
        return;
    }

    // Set up message callback for logging
    canDriver.setMessageCallback([](const CANMessage& msg) {
        // Log every message
        canLogger.logMessage(msg);

        // Parse battery data
        CANBatteryData battData;
        if (canParser.parseMessage(msg, battData)) {
            // Successfully parsed - will be handled in CAN task
        }
    });

    LOG_INFO("CAN bus initialized at %u kbps", bitrate / 1000);
}

void setupSensors() {
    LOG_INFO("Initializing sensors...");
    // TODO: Initialize ADC and sensors
    LOG_INFO("Sensors initialized (placeholder)");
}

void setupNetwork() {
    LOG_INFO("Initializing network...");

    const Settings& settings = settingsManager.getSettings();

    // Initialize WiFi manager
    wifiManager.begin();
    wifiManager.setAutoReconnect(true);

    // Set up WiFi state callback
    wifiManager.setStateCallback([](WiFiState state) {
        switch (state) {
            case WiFiState::CONNECTED:
                digitalWrite(PIN_STATUS_LED, HIGH);
                LOG_INFO("WiFi connected: %s", wifiManager.getLocalIP().toString().c_str());
                break;
            case WiFiState::DISCONNECTED:
                LOG_WARN("WiFi disconnected");
                digitalWrite(PIN_STATUS_LED, LOW);
                break;
            case WiFiState::CONNECTING:
                digitalWrite(PIN_STATUS_LED, LOW);
                break;
            case WiFiState::AP_MODE:
                LOG_INFO("AP Mode active: %s", wifiManager.getAPIP().toString().c_str());
                break;
            default:
                break;
        }
    });

    // Generate AP SSID with unique suffix from MAC
    String ap_ssid = String(WIFI_AP_SSID_PREFIX) + WiFi.macAddress().substring(12);
    ap_ssid.replace(":", "");

    // Determine connection mode
    if (strlen(settings.wifi_ssid) > 0) {
        // Try STA mode with configured credentials
        LOG_INFO("Connecting to WiFi: %s", settings.wifi_ssid);

        // Start AP+STA mode - this allows:
        // 1. Connection to home WiFi if credentials are correct
        // 2. Configuration portal via AP if connection fails
        wifiManager.startAPSTA(
            settings.wifi_ssid,
            settings.wifi_password,
            ap_ssid.c_str(),
            WIFI_AP_PASSWORD
        );

        // Wait for STA connection or timeout
        Serial.print("[WiFi] Waiting for connection");
        uint32_t start = millis();
        while (!wifiManager.isConnected() && (millis() - start) < WIFI_CONNECTION_TIMEOUT) {
            delay(100);
            if ((millis() - start) % 1000 < 100) Serial.print(".");
        }
        Serial.println();

        if (wifiManager.isConnected()) {
            LOG_INFO("Connected to %s, IP: %s", settings.wifi_ssid, wifiManager.getLocalIP().toString().c_str());
        } else {
            LOG_WARN("STA connection failed, AP mode available for configuration");
        }
    } else {
        // No credentials configured, start AP only
        LOG_INFO("No WiFi configured, starting AP mode...");
        wifiManager.startAP(ap_ssid.c_str(), WIFI_AP_PASSWORD);
        LOG_INFO("Connect to '%s' to configure", ap_ssid.c_str());
    }

    LOG_INFO("Network initialized");
}

void setupWebServer() {
    LOG_INFO("Initializing web server...");

    // Initialize web server with dependencies
    webServer.begin(&settingsManager, &batteryManager, &canLogger);

    // Set up WebSocket client callback
    webServer.setClientCallback([](uint32_t client_id, bool connected) {
        LOG_INFO("WebSocket client %u %s", client_id, connected ? "connected" : "disconnected");
    });

    // Wire up remote logger to broadcast via WebSocket
    remoteLog.setBroadcastCallback([](const LogEntry& entry) {
        webServer.broadcastLog(entry);
    });

    LOG_INFO("Web server started on port %d", WEB_SERVER_PORT);
}

// FreeRTOS Tasks
void canTask(void* parameter) {
    LOG_INFO("CAN task started");

    CANMessage msg;
    CANBatteryData battData;
    uint32_t last_stats_print = 0;

    while (true) {
        // Process received CAN messages
        while (canDriver.receiveMessage(msg, 0)) {
            // Parse the message
            if (canParser.parseMessage(msg, battData)) {
                // Update battery module with parsed data
                if (battData.valid && battData.battery_id < MAX_BATTERY_MODULES) {
                    BatteryModule* battery = batteryManager.getBattery(battData.battery_id);
                    if (battery != nullptr) {
                        battery->updateFromCAN(battData);
                    }
                }
            }
        }

        // Periodic flush of CAN log
        static uint32_t last_flush = 0;
        if (millis() - last_flush > 5000) {  // Every 5 seconds
            canLogger.flush();
            last_flush = millis();
        }

        // Print CAN statistics every 30 seconds
        if (millis() - last_stats_print > 30000) {
            const CANStats& stats = canDriver.getStats();
            LOG_DEBUG("CAN Stats - RX: %u, TX: %u, Dropped: %u, Errors: %u",
                      stats.rx_count, stats.tx_count, stats.rx_dropped, stats.error_count);
            LOG_DEBUG("CAN Logger - Messages: %u, Dropped: %u, Size: %d bytes",
                      canLogger.getMessageCount(), canLogger.getDroppedCount(),
                      canLogger.getLogSize());
            last_stats_print = millis();
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms cycle
    }
}

void sensorTask(void* parameter) {
    LOG_INFO("Sensor task started");

    const Settings& settings = settingsManager.getSettings();
    TickType_t sampleInterval = pdMS_TO_TICKS(settings.sample_interval_ms);

    while (true) {
        // TODO: Read all ADC sensors
        // - Current sensors (ACS712)
        // - Voltage sensors
        // - Update battery modules

        // Blink status LED
        static bool ledState = false;
        digitalWrite(PIN_STATUS_LED, ledState);
        ledState = !ledState;

        vTaskDelay(sampleInterval);
    }
}

void networkTask(void* parameter) {
    LOG_INFO("Network task started");

    const Settings& settings = settingsManager.getSettings();
    TickType_t webRefreshInterval = pdMS_TO_TICKS(settings.web_refresh_ms);

    uint32_t last_battery_broadcast = 0;
    uint32_t last_system_broadcast = 0;
    uint32_t last_wifi_check = 0;

    while (true) {
        uint32_t now = millis();

        // Update WiFi manager (handles auto-reconnect)
        if (now - last_wifi_check > 1000) {  // Check every second
            wifiManager.update();
            last_wifi_check = now;
        }

        // Only broadcast if WiFi is connected
        if (wifiManager.isConnected() || wifiManager.isAPActive()) {
            // Broadcast battery updates via WebSocket
            if (now - last_battery_broadcast > settings.web_refresh_ms) {
                webServer.broadcastBatteryUpdate();
                last_battery_broadcast = now;
            }

            // Broadcast system status less frequently (every 5 seconds)
            if (now - last_system_broadcast > 5000) {
                webServer.broadcastSystemStatus();
                last_system_broadcast = now;
            }
        }

        // TODO: Handle MQTT publishing (when implemented)
        // - Publish battery status to MQTT broker
        // - Publish system status

        vTaskDelay(webRefreshInterval);
    }
}
