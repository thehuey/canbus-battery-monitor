/*
 * Battery Module Test Example
 *
 * This example demonstrates battery module and manager functionality
 * with simulated data updates.
 */

#include <Arduino.h>
#include "../src/config/config.h"
#include "../src/battery/battery_module.h"
#include "../src/battery/battery_manager.h"
#include "../src/can/can_message.h"

BatteryManager batteryManager;

// Simulation variables
float simVoltage = 52.0f;
float simCurrent = 0.0f;
uint8_t simSOC = 100;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n========================================");
    Serial.println("Battery Module Test");
    Serial.println("========================================\n");

    // Test 1: Initialize battery manager with 2 batteries
    Serial.println("Test 1: Initializing BatteryManager with 2 batteries...");
    batteryManager.begin(2);
    Serial.println("✓ BatteryManager initialized\n");

    // Test 2: Configure battery names
    Serial.println("Test 2: Configuring battery names...");
    batteryManager.setBatteryName(0, "Front Battery");
    batteryManager.setBatteryName(1, "Rear Battery");
    Serial.println("✓ Battery names configured\n");

    // Test 3: Manual sensor updates
    Serial.println("Test 3: Testing manual sensor updates...");
    BatteryModule* frontBattery = batteryManager.getBattery(0);
    if (frontBattery) {
        frontBattery->updateVoltage(52.4f);
        frontBattery->updateCurrent(3.5f);
        Serial.printf("✓ Front battery updated: %.2fV, %.2fA, %.2fW\n",
                     frontBattery->getVoltage(),
                     frontBattery->getCurrent(),
                     frontBattery->getPower());
    }

    BatteryModule* rearBattery = batteryManager.getBattery(1);
    if (rearBattery) {
        rearBattery->updateVoltage(51.8f);
        rearBattery->updateCurrent(2.1f);
        Serial.printf("✓ Rear battery updated: %.2fV, %.2fA, %.2fW\n",
                     rearBattery->getVoltage(),
                     rearBattery->getCurrent(),
                     rearBattery->getPower());
    }
    Serial.println();

    // Test 4: CAN data updates
    Serial.println("Test 4: Testing CAN data updates...");
    CANBatteryData canData;
    canData.battery_id = 0;
    canData.pack_voltage = 52.0f;
    canData.pack_current = 4.2f;
    canData.soc = 85;
    canData.temp1 = 25.5f;
    canData.temp2 = 27.0f;
    canData.status_flags = CANStatusFlags::DISCHARGING;
    canData.valid = true;

    if (frontBattery) {
        frontBattery->updateFromCAN(canData);
        Serial.printf("✓ Front battery CAN update: SOC=%d%%, Temp1=%.1f°C, Temp2=%.1f°C\n",
                     frontBattery->getSOC(),
                     frontBattery->getTemp1(),
                     frontBattery->getTemp2());
    }
    Serial.println();

    // Test 5: Aggregate calculations
    Serial.println("Test 5: Testing aggregate calculations...");
    Serial.printf("Total Power: %.2f W\n", batteryManager.getTotalPower());
    Serial.printf("Total Current: %.2f A\n", batteryManager.getTotalCurrent());
    Serial.printf("Average Voltage: %.2f V\n", batteryManager.getAverageVoltage());
    Serial.println();

    // Test 6: Status flags
    Serial.println("Test 6: Testing status flags...");
    if (frontBattery) {
        uint8_t flags = frontBattery->getStatusFlags();
        Serial.printf("Status flags: 0x%02X\n", flags);

        if (flags & CANStatusFlags::DISCHARGING) {
            Serial.println("  ✓ Battery is discharging");
        }
        if (flags & CANStatusFlags::CHARGING) {
            Serial.println("  ✓ Battery is charging");
        }
        if (flags & CANStatusFlags::ERROR) {
            Serial.println("  ✗ Battery error detected!");
        }
    }
    Serial.println();

    // Test 7: Health check
    Serial.println("Test 7: Testing health monitoring...");
    if (batteryManager.allBatteriesHealthy()) {
        Serial.println("✓ All batteries healthy");
    } else {
        Serial.printf("✗ %d battery error(s) detected\n",
                     batteryManager.getErrorCount());
    }
    Serial.println();

    // Test 8: Data freshness
    Serial.println("Test 8: Testing data freshness...");
    if (frontBattery && frontBattery->isDataFresh(5000)) {
        Serial.println("✓ Battery data is fresh (<5 seconds old)");
        Serial.printf("  Last update: %lu ms ago\n",
                     millis() - frontBattery->getLastUpdate());
    }
    Serial.println();

    // Test 9: Enable/disable battery
    Serial.println("Test 9: Testing enable/disable...");
    batteryManager.enableBattery(1, false);
    Serial.printf("Rear battery disabled\n");
    Serial.printf("Total Power (with 1 battery): %.2f W\n",
                 batteryManager.getTotalPower());

    batteryManager.enableBattery(1, true);
    Serial.printf("Rear battery enabled\n");
    Serial.printf("Total Power (with 2 batteries): %.2f W\n",
                 batteryManager.getTotalPower());
    Serial.println();

    Serial.println("========================================");
    Serial.println("Setup complete!");
    Serial.println("Starting simulation...");
    Serial.println("========================================\n");
}

void loop() {
    // Simulate discharge cycle
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate > 1000) {  // Update every second
        // Simulate voltage drop and current draw
        simVoltage -= 0.01f;  // Slow voltage drop
        simCurrent = 3.0f + random(0, 20) / 10.0f;  // 3.0-5.0A
        simSOC = constrain(simSOC - 1, 0, 100);

        // Simulate temperature rise
        float temp1 = 25.0f + random(0, 50) / 10.0f;  // 25-30°C
        float temp2 = 26.0f + random(0, 50) / 10.0f;  // 26-31°C

        // Update front battery
        BatteryModule* frontBatt = batteryManager.getBattery(0);
        if (frontBatt) {
            frontBatt->updateVoltage(simVoltage);
            frontBatt->updateCurrent(simCurrent);

            CANBatteryData canData;
            canData.battery_id = 0;
            canData.pack_voltage = simVoltage;
            canData.pack_current = simCurrent;
            canData.soc = simSOC;
            canData.temp1 = temp1;
            canData.temp2 = temp2;
            canData.status_flags = CANStatusFlags::DISCHARGING;
            canData.valid = true;

            frontBatt->updateFromCAN(canData);
        }

        // Update rear battery (slightly different values)
        BatteryModule* rearBatt = batteryManager.getBattery(1);
        if (rearBatt) {
            rearBatt->updateVoltage(simVoltage - 0.5f);
            rearBatt->updateCurrent(simCurrent * 0.8f);

            CANBatteryData canData;
            canData.battery_id = 1;
            canData.pack_voltage = simVoltage - 0.5f;
            canData.pack_current = simCurrent * 0.8f;
            canData.soc = simSOC - 2;
            canData.temp1 = temp1 + 1.0f;
            canData.temp2 = temp2 + 1.0f;
            canData.status_flags = CANStatusFlags::DISCHARGING;
            canData.valid = true;

            rearBatt->updateFromCAN(canData);
        }

        lastUpdate = millis();
    }

    // Print detailed status every 5 seconds
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 5000) {
        Serial.println("\n========== Battery Status ==========");

        for (uint8_t i = 0; i < batteryManager.getActiveBatteryCount(); i++) {
            const BatteryModule* batt = batteryManager.getBattery(i);
            if (batt && batt->isEnabled()) {
                Serial.printf("\n%s:\n", batt->getName());
                Serial.printf("  Voltage:  %.2f V\n", batt->getVoltage());
                Serial.printf("  Current:  %.2f A\n", batt->getCurrent());
                Serial.printf("  Power:    %.2f W\n", batt->getPower());
                Serial.printf("  SOC:      %d%%\n", batt->getSOC());
                Serial.printf("  Temp1:    %.1f°C\n", batt->getTemp1());
                Serial.printf("  Temp2:    %.1f°C\n", batt->getTemp2());
                Serial.printf("  Status:   ");

                uint8_t flags = batt->getStatusFlags();
                if (flags & CANStatusFlags::CHARGING) Serial.print("CHARGING ");
                if (flags & CANStatusFlags::DISCHARGING) Serial.print("DISCHARGING ");
                if (flags & CANStatusFlags::BALANCING) Serial.print("BALANCING ");
                if (flags & CANStatusFlags::ERROR) Serial.print("ERROR ");
                Serial.println();

                Serial.printf("  Has CAN:  %s\n", batt->hasCANData() ? "Yes" : "No");
                Serial.printf("  Fresh:    %s\n", batt->isDataFresh(5000) ? "Yes" : "No");
            }
        }

        Serial.println("\nAggregates:");
        Serial.printf("  Total Power:    %.2f W\n", batteryManager.getTotalPower());
        Serial.printf("  Total Current:  %.2f A\n", batteryManager.getTotalCurrent());
        Serial.printf("  Average Voltage: %.2f V\n", batteryManager.getAverageVoltage());
        Serial.printf("  Health:         %s\n",
                     batteryManager.allBatteriesHealthy() ? "OK" : "ISSUES");

        Serial.println("====================================\n");

        lastPrint = millis();

        // Check for warnings
        if (simVoltage < 42.0f) {
            Serial.println("⚠️  WARNING: Low voltage detected!");
        }
        if (simSOC < 20) {
            Serial.println("⚠️  WARNING: Low state of charge!");
        }
    }

    // Reset simulation at end of discharge
    if (simSOC <= 0) {
        Serial.println("\n*** Resetting simulation (battery recharged) ***\n");
        simVoltage = 52.0f;
        simSOC = 100;
        delay(2000);
    }

    delay(10);
}
