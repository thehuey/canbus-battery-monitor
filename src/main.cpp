#include <Arduino.h>
#include "config/config.h"
#include "config/settings.h"
#include "battery/battery_manager.h"
#include "can/can_driver.h"
#include "can/can_parser.h"
#include "can/can_logger.h"

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
    Serial.println("\n\n=================================");
    Serial.println("eBike Battery CANBUS Monitor");
    Serial.println("=================================\n");

    // Configure GPIO pins
    setupPins();

    // Load settings from NVS
    Serial.println("Loading settings...");
    if (!settingsManager.begin()) {
        Serial.println("Warning: Using default settings");
    }

    // Print current settings
    settingsManager.printSettings();

    // Initialize battery manager
    const Settings& settings = settingsManager.getSettings();
    Serial.printf("Initializing %d battery module(s)...\n", settings.num_batteries);
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
    Serial.println("\nStarting tasks...");

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

    Serial.println("System initialized successfully!");
    Serial.println("=================================\n");
}

void loop() {
    // Main loop runs on core 1
    // Most work is done in FreeRTOS tasks

    // Update battery manager
    batteryManager.update();

    // Monitor battery health
    static uint32_t lastHealthCheck = 0;
    if (millis() - lastHealthCheck > 30000) {  // Every 30 seconds
        if (!batteryManager.allBatteriesHealthy()) {
            uint8_t errorCount = batteryManager.getErrorCount();
            Serial.printf("Warning: %d battery error(s) detected\n", errorCount);

            // Print detailed status
            for (uint8_t i = 0; i < batteryManager.getActiveBatteryCount(); i++) {
                const BatteryModule* battery = batteryManager.getBattery(i);
                if (battery != nullptr && battery->isEnabled()) {
                    if (battery->hasError() || !battery->isDataFresh(10000)) {
                        Serial.printf("  Battery %d (%s): ", i, battery->getName());
                        if (!battery->isDataFresh(10000)) {
                            Serial.println("STALE DATA");
                        } else if (battery->hasError()) {
                            Serial.println("ERROR FLAG SET");
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
            Serial.printf("Warning: Low heap memory: %d bytes\n", freeHeap);
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
    Serial.println("GPIO pins configured");
}

void setupCANBus() {
    Serial.println("Initializing CAN bus...");

    // Initialize CAN logger
    if (!canLogger.begin("/canlog.csv")) {
        Serial.println("Warning: CAN logger initialization failed");
    }

    // Initialize CAN driver
    uint32_t bitrate = settingsManager.getSettings().can_bitrate;
    if (!canDriver.begin(bitrate)) {
        Serial.println("Error: CAN driver initialization failed!");
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

    Serial.println("CAN bus initialized successfully");
}

void setupSensors() {
    Serial.println("Initializing sensors...");
    // TODO: Initialize ADC and sensors
    Serial.println("Sensors initialized (placeholder)");
}

void setupNetwork() {
    Serial.println("Initializing network...");
    // TODO: Initialize WiFi and MQTT
    Serial.println("Network initialized (placeholder)");
}

void setupWebServer() {
    Serial.println("Initializing web server...");
    // TODO: Initialize async web server
    Serial.println("Web server initialized (placeholder)");
}

// FreeRTOS Tasks
void canTask(void* parameter) {
    Serial.println("CAN task started");

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
            Serial.printf("CAN Stats - RX: %u, TX: %u, Dropped: %u, Errors: %u\n",
                         stats.rx_count, stats.tx_count, stats.rx_dropped, stats.error_count);
            Serial.printf("CAN Logger - Messages: %u, Dropped: %u, Size: %d bytes\n",
                         canLogger.getMessageCount(), canLogger.getDroppedCount(),
                         canLogger.getLogSize());
            last_stats_print = millis();
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms cycle
    }
}

void sensorTask(void* parameter) {
    Serial.println("Sensor task started");

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
    Serial.println("Network task started");

    const Settings& settings = settingsManager.getSettings();
    TickType_t publishInterval = pdMS_TO_TICKS(settings.publish_interval_ms);

    while (true) {
        // TODO: Handle network operations
        // - Check WiFi connection
        // - Publish MQTT messages
        // - Send WebSocket updates

        vTaskDelay(publishInterval);
    }
}
