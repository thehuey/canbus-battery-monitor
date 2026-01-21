#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <Arduino.h>
#include "can_message.h"
#include "../utils/ring_buffer.h"

// CAN driver status
enum class CANStatus {
    UNINITIALIZED,
    RUNNING,
    BUS_OFF,
    ERROR
};

// CAN statistics
struct CANStats {
    uint32_t rx_count;
    uint32_t tx_count;
    uint32_t rx_dropped;
    uint32_t tx_failed;
    uint32_t bus_off_count;
    uint32_t error_count;
    uint32_t last_error_code;

    CANStats() : rx_count(0), tx_count(0), rx_dropped(0), tx_failed(0),
                 bus_off_count(0), error_count(0), last_error_code(0) {}
};

// CAN Driver class
class CANDriver {
public:
    CANDriver();

    // Initialization
    bool begin(uint32_t bitrate = 500000);
    void end();

    // Message operations
    bool sendMessage(const CANMessage& msg);
    bool receiveMessage(CANMessage& msg, uint32_t timeout_ms = 0);
    size_t available() const;

    // Status and control
    CANStatus getStatus() const { return status; }
    const CANStats& getStats() const { return stats; }
    void resetStats();

    // Diagnostic info
    const char* getStatusString() const;
    bool getDiagnostics(char* buffer, size_t size) const;

    // Recovery
    bool recoverBusOff();

    // Message filtering (hardware filters)
    bool setFilter(uint32_t id, uint32_t mask, bool extended = false);
    void clearFilters();

    // Callbacks for received messages
    typedef void (*MessageCallback)(const CANMessage& msg);
    void setMessageCallback(MessageCallback callback);

    // Test/Ping functions
    bool sendPing();  // Send a test message to verify transceiver is working
    void enablePeriodicPing(uint32_t interval_ms);
    void disablePeriodicPing();

private:
    CANStatus status;
    CANStats stats;
    MessageCallback msg_callback;

    // Internal message queue
    static constexpr size_t RX_QUEUE_SIZE = 100;
    RingBuffer<CANMessage, RX_QUEUE_SIZE> rx_queue;

    // TWAI driver state
    bool is_initialized;
    uint32_t current_bitrate;

    // Internal handlers
    static void rxTaskFunc(void* parameter);
    void processReceivedMessages();
    void handleBusError();

    // Task handle
    TaskHandle_t rx_task_handle;

    // Ping/heartbeat state
    bool ping_enabled;
    uint32_t ping_interval_ms;
    uint32_t last_ping_time;
    uint8_t ping_counter;
};

// Global CAN driver instance
extern CANDriver canDriver;

#endif // CAN_DRIVER_H
