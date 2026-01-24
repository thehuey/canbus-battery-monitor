#ifndef CAN_LOGGER_H
#define CAN_LOGGER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include "can_message.h"
#include "../utils/ring_buffer.h"

// CAN logger for SPIFFS storage
class CANLogger {
public:
    CANLogger();

    // Initialization
    bool begin(const char* log_file = "/canlog.csv");
    void end();

    // Logging operations
    bool logMessage(const CANMessage& msg);
    bool flush();  // Write buffered messages to file

    // Log management
    bool clear();  // Clear the log file
    size_t getLogSize();  // Get log file size in bytes
    bool exportCSV(Stream& output);  // Export log as CSV
    bool exportFiltered(Stream& output, uint32_t filter_id);  // Export filtered by ID

    // Memory-only logging (for web interface)
    bool getRecentMessages(CANMessage* buffer, size_t& count, size_t max_count);
    bool getFilteredMessages(CANMessage* buffer, size_t& count, size_t max_count, uint32_t filter_id);

    // Statistics
    uint32_t getMessageCount() const { return message_count; }
    uint32_t getDroppedCount() const { return dropped_count; }

    // Configuration
    void setAutoFlush(bool enable) { auto_flush = enable; }
    void setFlushInterval(uint32_t interval_ms) { flush_interval_ms = interval_ms; }

private:
    // File operations
    bool openLogFile(const char* mode);
    void closeLogFile();
    bool writeMessageToFile(const CANMessage& msg);
    bool checkAndRotate();  // Check if rotation is needed

    // In-memory buffer for recent messages
    static constexpr size_t MEMORY_BUFFER_SIZE = 2000;
    RingBuffer<CANMessage, MEMORY_BUFFER_SIZE> memory_buffer;

    // Write buffer for batching
    static constexpr size_t WRITE_BUFFER_SIZE = 100;
    RingBuffer<CANMessage, WRITE_BUFFER_SIZE> write_buffer;

    // State
    bool is_initialized;
    char log_filename[32];
    File log_file;
    uint32_t message_count;
    uint32_t dropped_count;
    uint32_t last_flush_time;

    // Configuration
    bool auto_flush;
    uint32_t flush_interval_ms;

    // Helper functions
    void formatMessageCSV(const CANMessage& msg, char* buffer, size_t size);
};

// Global logger instance
extern CANLogger canLogger;

#endif // CAN_LOGGER_H
