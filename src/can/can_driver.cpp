#include "can_driver.h"
#include "../config/config.h"
#include "../utils/remote_log.h"
#include "driver/twai.h"

// Global instance
CANDriver canDriver;

CANDriver::CANDriver()
    : status(CANStatus::UNINITIALIZED),
      msg_callback(nullptr),
      is_initialized(false),
      current_bitrate(0),
      rx_task_handle(nullptr),
      ping_enabled(false),
      ping_interval_ms(1000),
      last_ping_time(0),
      ping_counter(0) {
}

bool CANDriver::begin(uint32_t bitrate) {
    if (is_initialized) {
        LOG_INFO("CANDriver: Already initialized");
        return true;
    }

    LOG_INFO("CANDriver: Initializing at %d bps...", bitrate);

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
            LOG_WARN("CANDriver: Unsupported bitrate %d, using 500kbps", bitrate);
            t_config = TWAI_TIMING_CONFIG_500KBITS();
            bitrate = 500000;
            break;
    }

    // Filter configuration - accept all messages
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install TWAI driver
    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        LOG_ERROR("CANDriver: Failed to install driver: %d", err);
        return false;
    }

    // Start TWAI driver
    err = twai_start();
    if (err != ESP_OK) {
        LOG_ERROR("CANDriver: Failed to start driver: %d", err);
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

    LOG_INFO("CANDriver: Initialized successfully at %d bps", bitrate);
    return true;
}

void CANDriver::end() {
    if (!is_initialized) {
        return;
    }

    LOG_INFO("CANDriver: Shutting down...");

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

    LOG_INFO("CANDriver: Shutdown complete");
}

bool CANDriver::sendMessage(const CANMessage& msg) {
    if (!is_initialized) {
        LOG_ERROR("CAN TX failed: driver not initialized");
        return false;
    }

    if (status != CANStatus::RUNNING) {
        LOG_ERROR("CAN TX failed: bus status is %d (not RUNNING)", static_cast<int>(status));
        return false;
    }

    // Convert our message format to TWAI format
    twai_message_t twai_msg;
    twai_msg.identifier = msg.id;
    twai_msg.data_length_code = msg.dlc;
    twai_msg.rtr = msg.rtr ? 1 : 0;
    twai_msg.extd = msg.extended ? 1 : 0;
    memcpy(twai_msg.data, msg.data, msg.dlc);

    // Try to send (non-blocking with 10ms timeout)
    esp_err_t err = twai_transmit(&twai_msg, pdMS_TO_TICKS(10));
    if (err == ESP_OK) {
        stats.tx_count++;
        return true;
    } else {
        stats.tx_failed++;

        // Log specific error
        if (err == ESP_ERR_TIMEOUT) {
            LOG_WARN("CAN TX timeout: TX queue full (increase CAN_TX_QUEUE_SIZE)");
        } else if (err == ESP_ERR_INVALID_STATE) {
            LOG_ERROR("CAN TX failed: invalid state (bus-off or not started)");
        } else {
            LOG_ERROR("CAN TX failed: error code %d", err);
        }
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

    LOG_WARN("CANDriver: Attempting bus-off recovery...");

    esp_err_t err = twai_initiate_recovery();
    if (err == ESP_OK) {
        // Wait for recovery
        vTaskDelay(pdMS_TO_TICKS(100));

        err = twai_start();
        if (err == ESP_OK) {
            status = CANStatus::RUNNING;
            stats.bus_off_count++;
            LOG_INFO("CANDriver: Recovery successful");
            return true;
        }
    }

    LOG_ERROR("CANDriver: Recovery failed");
    return false;
}

bool CANDriver::setFilter(uint32_t id, uint32_t mask, bool extended) {
    // Note: TWAI filters must be set before driver installation
    // This function is a placeholder for future dynamic filter support
    LOG_WARN("CANDriver: Dynamic filters not yet implemented");
    return false;
}

void CANDriver::clearFilters() {
    // Placeholder - filters are set during initialization
}

void CANDriver::setMessageCallback(MessageCallback callback) {
    msg_callback = callback;
}

bool CANDriver::sendPing() {
    if (!is_initialized || status != CANStatus::RUNNING) {
        return false;
    }

    CANMessage ping_msg;
    ping_msg.id = 0x404;  // Ping message ID
    ping_msg.dlc = 8;
    ping_msg.extended = false;
    ping_msg.rtr = false;

    // Create alternating F0F0 pattern
    // Use ping_counter to alternate between F0 and 0F on odd/even pings
    uint8_t pattern = (ping_counter & 1) ? 0xF0 : 0x0F;
    for (int i = 0; i < 8; i++) {
        ping_msg.data[i] = pattern;
        pattern = ~pattern;  // Alternate for each byte
    }

    ping_counter++;

    if (sendMessage(ping_msg)) {
        LOG_DEBUG("[CAN] Ping sent: ID=0x404, counter=%d", ping_counter);
        return true;
    } else {
        LOG_WARN("[CAN] Ping failed to send");
        return false;
    }
}

void CANDriver::enablePeriodicPing(uint32_t interval_ms) {
    ping_enabled = true;
    ping_interval_ms = interval_ms;
    last_ping_time = millis();
    LOG_INFO("[CAN] Periodic ping enabled (interval: %d ms)", interval_ms);
}

void CANDriver::disablePeriodicPing() {
    ping_enabled = false;
    LOG_INFO("[CAN] Periodic ping disabled");
}

void CANDriver::rxTaskFunc(void* parameter) {
    CANDriver* driver = static_cast<CANDriver*>(parameter);

    LOG_INFO("CANDriver: RX task started");

    while (true) {
        driver->processReceivedMessages();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void CANDriver::processReceivedMessages() {
    // Handle periodic ping if enabled (only if bus is RUNNING)
    if (ping_enabled && status == CANStatus::RUNNING) {
        uint32_t now = millis();
        if (now - last_ping_time >= ping_interval_ms) {
            sendPing();
            last_ping_time = now;
        }
    }

    // Periodic status check (every 10 seconds)
    static uint32_t last_status_log = 0;
    uint32_t now = millis();
    if (now - last_status_log >= 10000) {
        twai_status_info_t status_info;
        if (twai_get_status_info(&status_info) == ESP_OK) {
            if (status_info.state != TWAI_STATE_RUNNING) {
                LOG_WARN("CAN Bus State: %s, TX Errors: %u, RX Errors: %u, Queued: %u",
                    status_info.state == TWAI_STATE_BUS_OFF ? "BUS_OFF" :
                    status_info.state == TWAI_STATE_RECOVERING ? "RECOVERING" :
                    status_info.state == TWAI_STATE_STOPPED ? "STOPPED" : "UNKNOWN",
                    status_info.tx_error_counter,
                    status_info.rx_error_counter,
                    status_info.msgs_to_tx);
            } else {
                LOG_DEBUG("CAN Bus: RUNNING, TX:%u RX:%u Errors:TX=%u,RX=%u",
                    stats.tx_count, stats.rx_count,
                    status_info.tx_error_counter,
                    status_info.rx_error_counter);
            }
        }
        last_status_log = now;
    }

    twai_message_t twai_msg;

    // Read all available messages
    uint32_t msgs_this_cycle = 0;
    while (twai_receive(&twai_msg, 0) == ESP_OK) {
        msgs_this_cycle++;

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
            LOG_WARN("CAN RX buffer full, dropped message ID=0x%03X", msg.id);
        } else {
            stats.rx_count++;

            // Log first few messages to confirm reception
            if (stats.rx_count <= 5) {
                LOG_INFO("CAN RX #%u: ID=0x%03X DLC=%u", stats.rx_count, msg.id, msg.dlc);
            }

            // Call callback if registered
            if (msg_callback != nullptr) {
                msg_callback(msg);
            }
        }
    }

    // Log burst activity
    if (msgs_this_cycle > 10) {
        LOG_DEBUG("CAN: Processed %u messages in one cycle", msgs_this_cycle);
    }

    // Check for bus errors
    twai_status_info_t status_info;
    if (twai_get_status_info(&status_info) == ESP_OK) {
        if (status_info.state == TWAI_STATE_BUS_OFF) {
            if (status != CANStatus::BUS_OFF) {
                LOG_ERROR("CANDriver: Bus-off detected! TX errors=%u, RX errors=%u",
                    status_info.tx_error_counter, status_info.rx_error_counter);
                LOG_ERROR("This usually means: no termination resistor, no other CAN device, or wrong bitrate");
                status = CANStatus::BUS_OFF;
                handleBusError();
            }
        } else if (status_info.state == TWAI_STATE_RUNNING) {
            if (status == CANStatus::BUS_OFF) {
                LOG_INFO("CANDriver: Bus recovered to RUNNING state");
                status = CANStatus::RUNNING;
            }
        }
    }
}

void CANDriver::handleBusError() {
    stats.error_count++;

    // Attempt automatic recovery
    LOG_WARN("CANDriver: Attempting automatic recovery...");
    if (recoverBusOff()) {
        LOG_INFO("CANDriver: Automatic recovery successful");
    } else {
        LOG_ERROR("CANDriver: Automatic recovery failed, manual intervention required");
    }
}

const char* CANDriver::getStatusString() const {
    switch (status) {
        case CANStatus::UNINITIALIZED: return "UNINITIALIZED";
        case CANStatus::RUNNING: return "RUNNING";
        case CANStatus::BUS_OFF: return "BUS_OFF";
        case CANStatus::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

bool CANDriver::getDiagnostics(char* buffer, size_t size) const {
    if (!buffer || size < 512) return false;

    // Get TWAI status
    twai_status_info_t status_info;
    bool has_status = (twai_get_status_info(&status_info) == ESP_OK);

    snprintf(buffer, size,
        "CAN Driver Status:\n"
        "  Initialized: %s\n"
        "  Status: %s\n"
        "  Bitrate: %u bps\n"
        "\n"
        "Statistics:\n"
        "  RX Count: %u\n"
        "  TX Count: %u\n"
        "  RX Dropped: %u\n"
        "  TX Failed: %u\n"
        "  Bus-off Count: %u\n"
        "  Error Count: %u\n"
        "\n"
        "TWAI Hardware:\n"
        "  State: %s\n"
        "  TX Queue: %u messages waiting\n"
        "  RX Queue: %u messages waiting\n"
        "  TX Error Counter: %u\n"
        "  RX Error Counter: %u\n"
        "  Bus Error Counter: %u\n",
        is_initialized ? "Yes" : "No",
        getStatusString(),
        current_bitrate,
        stats.rx_count,
        stats.tx_count,
        stats.rx_dropped,
        stats.tx_failed,
        stats.bus_off_count,
        stats.error_count,
        has_status ? (status_info.state == TWAI_STATE_RUNNING ? "RUNNING" :
                      status_info.state == TWAI_STATE_BUS_OFF ? "BUS_OFF" :
                      status_info.state == TWAI_STATE_RECOVERING ? "RECOVERING" :
                      status_info.state == TWAI_STATE_STOPPED ? "STOPPED" : "UNKNOWN") : "N/A",
        has_status ? status_info.msgs_to_tx : 0,
        has_status ? status_info.msgs_to_rx : 0,
        has_status ? status_info.tx_error_counter : 0,
        has_status ? status_info.rx_error_counter : 0,
        has_status ? status_info.bus_error_count : 0
    );

    return true;
}
