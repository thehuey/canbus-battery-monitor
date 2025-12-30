#include "can_driver.h"
#include "../config/config.h"
#include "driver/twai.h"

// Global instance
CANDriver canDriver;

CANDriver::CANDriver()
    : status(CANStatus::UNINITIALIZED),
      msg_callback(nullptr),
      is_initialized(false),
      current_bitrate(0),
      rx_task_handle(nullptr) {
}

bool CANDriver::begin(uint32_t bitrate) {
    if (is_initialized) {
        Serial.println("CANDriver: Already initialized");
        return true;
    }

    Serial.printf("CANDriver: Initializing at %d bps...\n", bitrate);

    // General configuration
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)PIN_CAN_TX,
        (gpio_num_t)PIN_CAN_RX,
        TWAI_MODE_NORMAL
    );
    g_config.rx_queue_len = CAN_RX_QUEUE_SIZE;
    g_config.tx_queue_len = CAN_TX_QUEUE_SIZE;

    // Timing configuration for specified bitrate
    twai_timing_config_t t_config;
    switch (bitrate) {
        case 500000:
            t_config = TWAI_TIMING_CONFIG_500KBITS();
            break;
        case 250000:
            t_config = TWAI_TIMING_CONFIG_250KBITS();
            break;
        case 125000:
            t_config = TWAI_TIMING_CONFIG_125KBITS();
            break;
        case 100000:
            t_config = TWAI_TIMING_CONFIG_100KBITS();
            break;
        case 1000000:
            t_config = TWAI_TIMING_CONFIG_1MBITS();
            break;
        default:
            Serial.printf("CANDriver: Unsupported bitrate %d, using 500kbps\n", bitrate);
            t_config = TWAI_TIMING_CONFIG_500KBITS();
            bitrate = 500000;
            break;
    }

    // Filter configuration - accept all messages
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install TWAI driver
    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        Serial.printf("CANDriver: Failed to install driver: %d\n", err);
        return false;
    }

    // Start TWAI driver
    err = twai_start();
    if (err != ESP_OK) {
        Serial.printf("CANDriver: Failed to start driver: %d\n", err);
        twai_driver_uninstall();
        return false;
    }

    is_initialized = true;
    current_bitrate = bitrate;
    status = CANStatus::RUNNING;
    resetStats();

    // Create RX task
    xTaskCreatePinnedToCore(
        rxTaskFunc,
        "CAN RX Task",
        4096,
        this,
        2,
        &rx_task_handle,
        0
    );

    Serial.printf("CANDriver: Initialized successfully at %d bps\n", bitrate);
    return true;
}

void CANDriver::end() {
    if (!is_initialized) {
        return;
    }

    Serial.println("CANDriver: Shutting down...");

    // Delete RX task
    if (rx_task_handle != nullptr) {
        vTaskDelete(rx_task_handle);
        rx_task_handle = nullptr;
    }

    // Stop and uninstall driver
    twai_stop();
    twai_driver_uninstall();

    is_initialized = false;
    status = CANStatus::UNINITIALIZED;

    Serial.println("CANDriver: Shutdown complete");
}

bool CANDriver::sendMessage(const CANMessage& msg) {
    if (!is_initialized || status != CANStatus::RUNNING) {
        return false;
    }

    // Convert our message format to TWAI format
    twai_message_t twai_msg;
    twai_msg.identifier = msg.id;
    twai_msg.data_length_code = msg.dlc;
    twai_msg.rtr = msg.rtr ? 1 : 0;
    twai_msg.extd = msg.extended ? 1 : 0;
    memcpy(twai_msg.data, msg.data, msg.dlc);

    // Try to send (non-blocking)
    esp_err_t err = twai_transmit(&twai_msg, pdMS_TO_TICKS(10));
    if (err == ESP_OK) {
        stats.tx_count++;
        return true;
    } else {
        stats.tx_failed++;
        return false;
    }
}

bool CANDriver::receiveMessage(CANMessage& msg, uint32_t timeout_ms) {
    // Try to get from our ring buffer first
    if (rx_queue.pop(msg)) {
        return true;
    }

    // If timeout is specified, wait a bit
    if (timeout_ms > 0) {
        uint32_t start = millis();
        while (millis() - start < timeout_ms) {
            if (rx_queue.pop(msg)) {
                return true;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    return false;
}

size_t CANDriver::available() const {
    return rx_queue.size();
}

void CANDriver::resetStats() {
    stats = CANStats();
}

bool CANDriver::recoverBusOff() {
    if (!is_initialized) {
        return false;
    }

    Serial.println("CANDriver: Attempting bus-off recovery...");

    esp_err_t err = twai_initiate_recovery();
    if (err == ESP_OK) {
        // Wait for recovery
        vTaskDelay(pdMS_TO_TICKS(100));

        err = twai_start();
        if (err == ESP_OK) {
            status = CANStatus::RUNNING;
            stats.bus_off_count++;
            Serial.println("CANDriver: Recovery successful");
            return true;
        }
    }

    Serial.println("CANDriver: Recovery failed");
    return false;
}

bool CANDriver::setFilter(uint32_t id, uint32_t mask, bool extended) {
    // Note: TWAI filters must be set before driver installation
    // This function is a placeholder for future dynamic filter support
    Serial.println("CANDriver: Dynamic filters not yet implemented");
    return false;
}

void CANDriver::clearFilters() {
    // Placeholder - filters are set during initialization
}

void CANDriver::setMessageCallback(MessageCallback callback) {
    msg_callback = callback;
}

void CANDriver::rxTaskFunc(void* parameter) {
    CANDriver* driver = static_cast<CANDriver*>(parameter);

    Serial.println("CANDriver: RX task started");

    while (true) {
        driver->processReceivedMessages();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void CANDriver::processReceivedMessages() {
    twai_message_t twai_msg;

    // Read all available messages
    while (twai_receive(&twai_msg, 0) == ESP_OK) {
        // Convert to our message format
        CANMessage msg;
        msg.id = twai_msg.identifier;
        msg.dlc = twai_msg.data_length_code;
        msg.extended = twai_msg.extd;
        msg.rtr = twai_msg.rtr;
        msg.timestamp = millis();
        memcpy(msg.data, twai_msg.data, twai_msg.data_length_code);

        // Add to ring buffer
        if (!rx_queue.push(msg)) {
            stats.rx_dropped++;
        } else {
            stats.rx_count++;

            // Call callback if registered
            if (msg_callback != nullptr) {
                msg_callback(msg);
            }
        }
    }

    // Check for bus errors
    twai_status_info_t status_info;
    if (twai_get_status_info(&status_info) == ESP_OK) {
        if (status_info.state == TWAI_STATE_BUS_OFF) {
            if (status != CANStatus::BUS_OFF) {
                Serial.println("CANDriver: Bus-off detected!");
                status = CANStatus::BUS_OFF;
                handleBusError();
            }
        } else if (status_info.state == TWAI_STATE_RUNNING) {
            if (status == CANStatus::BUS_OFF) {
                Serial.println("CANDriver: Bus recovered");
                status = CANStatus::RUNNING;
            }
        }
    }
}

void CANDriver::handleBusError() {
    stats.error_count++;

    // Attempt automatic recovery
    Serial.println("CANDriver: Attempting automatic recovery...");
    if (recoverBusOff()) {
        Serial.println("CANDriver: Automatic recovery successful");
    } else {
        Serial.println("CANDriver: Automatic recovery failed, manual intervention required");
    }
}
