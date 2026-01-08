#ifndef REMOTE_LOG_H
#define REMOTE_LOG_H

#include <Arduino.h>
#include <functional>

// Log levels
enum class LogLevel : uint8_t {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

// Single log entry
struct LogEntry {
    uint32_t timestamp;
    LogLevel level;
    char message[128];
};

// Callback for broadcasting logs
using LogBroadcastCallback = std::function<void(const LogEntry& entry)>;

// Configuration
#define LOG_BUFFER_SIZE 50  // Number of recent messages to keep

class RemoteLogger {
public:
    RemoteLogger();

    // Initialize the logger
    void begin();

    // Log methods
    void debug(const char* format, ...);
    void info(const char* format, ...);
    void warn(const char* format, ...);
    void error(const char* format, ...);
    void log(LogLevel level, const char* format, ...);

    // Set minimum level for remote output (Serial always gets everything)
    void setRemoteLevel(LogLevel level) { remote_min_level_ = level; }
    LogLevel getRemoteLevel() const { return remote_min_level_; }

    // Enable/disable Serial output
    void setSerialEnabled(bool enabled) { serial_enabled_ = enabled; }
    bool isSerialEnabled() const { return serial_enabled_; }

    // Set broadcast callback (called for each log message)
    void setBroadcastCallback(LogBroadcastCallback callback) { broadcast_callback_ = callback; }

    // Get recent log entries (for new client catch-up)
    // Returns number of entries copied
    size_t getRecentLogs(LogEntry* buffer, size_t max_entries) const;

    // Get total count of entries in buffer
    size_t getEntryCount() const;

    // Clear the log buffer
    void clear();

    // Get level name as string
    static const char* levelToString(LogLevel level);

private:
    void logImpl(LogLevel level, const char* format, va_list args);
    void addEntry(const LogEntry& entry);

    LogEntry buffer_[LOG_BUFFER_SIZE];
    size_t head_;           // Next write position
    size_t count_;          // Number of valid entries
    LogLevel remote_min_level_;
    bool serial_enabled_;
    LogBroadcastCallback broadcast_callback_;

    // Mutex for thread safety
    SemaphoreHandle_t mutex_;
};

// Global instance
extern RemoteLogger remoteLog;

// Convenience macros
#define LOG_DEBUG(fmt, ...) remoteLog.debug(fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  remoteLog.info(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  remoteLog.warn(fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) remoteLog.error(fmt, ##__VA_ARGS__)

#endif // REMOTE_LOG_H
