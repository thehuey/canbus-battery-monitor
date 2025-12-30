/*
 * CAN Driver Test Example
 *
 * This example demonstrates the CAN driver, parser, and logger functionality.
 * Connect your ESP32 to a CAN bus and observe the messages.
 *
 * Hardware required:
 * - ESP32 board
 * - TJA1050 CAN transceiver
 * - Connection to CAN bus or another CAN node
 */

#include <Arduino.h>
#include "../src/config/config.h"
#include "../src/can/can_driver.h"
#include "../src/can/can_parser.h"
#include "../src/can/can_logger.h"

CANParser parser;
uint32_t last_send = 0;
uint32_t send_counter = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n========================================");
    Serial.println("CAN Driver Test");
    Serial.println("========================================\n");

    // Test 1: Initialize CAN logger
    Serial.println("Test 1: Initializing CAN logger...");
    if (canLogger.begin("/canlog.csv")) {
        Serial.println("✓ CAN logger initialized");
    } else {
        Serial.println("✗ CAN logger initialization failed");
    }

    // Test 2: Initialize CAN driver
    Serial.println("\nTest 2: Initializing CAN driver at 500 kbps...");
    if (canDriver.begin(500000)) {
        Serial.println("✓ CAN driver initialized");
    } else {
        Serial.println("✗ CAN driver initialization failed");
        Serial.println("Check hardware connections!");
        while (1) delay(1000);
    }

    // Test 3: Set up message callback
    Serial.println("\nTest 3: Setting up message callback...");
    canDriver.setMessageCallback([](const CANMessage& msg) {
        // Log every message
        canLogger.logMessage(msg);

        // Print received message
        Serial.printf("RX: ID=0x%03X, DLC=%d, Data=", msg.id, msg.dlc);
        for (uint8_t i = 0; i < msg.dlc; i++) {
            Serial.printf("%02X ", msg.data[i]);
        }
        Serial.println();

        // Try to parse as battery data
        CANBatteryData battData;
        if (parser.parseMessage(msg, battData)) {
            Serial.printf("  ↳ Battery %d: %.1fV, %.1fA, SOC=%d%%, Temp=%.1f°C\n",
                         battData.battery_id,
                         battData.pack_voltage,
                         battData.pack_current,
                         battData.soc,
                         battData.temp1);
        }
    });
    Serial.println("✓ Callback registered");

    // Test 4: Configure logger
    Serial.println("\nTest 4: Configuring logger...");
    canLogger.setAutoFlush(true);
    canLogger.setFlushInterval(5000);  // Flush every 5 seconds
    Serial.println("✓ Logger configured (auto-flush every 5s)");

    Serial.println("\n========================================");
    Serial.println("Setup complete!");
    Serial.println("Listening for CAN messages...");
    Serial.println("Sending test messages every 2 seconds...");
    Serial.println("========================================\n");
}

void loop() {
    // Send test message every 2 seconds
    if (millis() - last_send > 2000) {
        CANMessage msg;
        msg.id = 0x100;  // Example battery status ID
        msg.dlc = 8;
        msg.extended = false;
        msg.rtr = false;

        // Simulate battery data
        // Pack voltage: 52.0V (520 in 0.1V units)
        uint16_t voltage = 520;
        msg.data[0] = voltage & 0xFF;
        msg.data[1] = (voltage >> 8) & 0xFF;

        // Pack current: 3.5A (32035 = 32000 + 35)
        int16_t current = 32035;
        msg.data[2] = current & 0xFF;
        msg.data[3] = (current >> 8) & 0xFF;

        // SOC: 85%
        msg.data[4] = 85;

        // Temperature 1: 25°C (65 = 25 + 40)
        msg.data[5] = 65;

        // Temperature 2: 27°C (67 = 27 + 40)
        msg.data[6] = 67;

        // Status flags: discharging
        msg.data[7] = CANStatusFlags::DISCHARGING;

        if (canDriver.sendMessage(msg)) {
            Serial.printf("TX: Sent test message #%lu\n", ++send_counter);
        } else {
            Serial.println("TX: Failed to send message");
        }

        last_send = millis();
    }

    // Print statistics every 10 seconds
    static uint32_t last_stats = 0;
    if (millis() - last_stats > 10000) {
        const CANStats& stats = canDriver.getStats();
        Serial.println("\n--- CAN Statistics ---");
        Serial.printf("RX Messages: %lu\n", stats.rx_count);
        Serial.printf("TX Messages: %lu\n", stats.tx_count);
        Serial.printf("RX Dropped: %lu\n", stats.rx_dropped);
        Serial.printf("TX Failed: %lu\n", stats.tx_failed);
        Serial.printf("Bus-off Count: %lu\n", stats.bus_off_count);
        Serial.printf("Errors: %lu\n", stats.error_count);
        Serial.printf("Status: ");
        switch (canDriver.getStatus()) {
            case CANStatus::RUNNING: Serial.println("RUNNING"); break;
            case CANStatus::BUS_OFF: Serial.println("BUS_OFF"); break;
            case CANStatus::ERROR: Serial.println("ERROR"); break;
            default: Serial.println("UNKNOWN"); break;
        }

        Serial.println("\n--- Logger Statistics ---");
        Serial.printf("Messages Logged: %lu\n", canLogger.getMessageCount());
        Serial.printf("Messages Dropped: %lu\n", canLogger.getDroppedCount());
        Serial.printf("Log File Size: %d bytes\n", canLogger.getLogSize());
        Serial.println();

        last_stats = millis();
    }

    // Export log to serial on command (send 'e' via serial)
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'e' || c == 'E') {
            Serial.println("\n--- Exporting CAN Log ---");
            canLogger.flush();
            canLogger.exportCSV(Serial);
            Serial.println("--- End of Log ---\n");
        } else if (c == 'c' || c == 'C') {
            Serial.println("\n--- Clearing CAN Log ---");
            canLogger.clear();
            Serial.println("Log cleared\n");
        } else if (c == 'r' || c == 'R') {
            Serial.println("\n--- Resetting Statistics ---");
            canDriver.resetStats();
            Serial.println("Statistics reset\n");
        }
    }

    delay(10);
}
