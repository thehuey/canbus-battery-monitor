#include "remote_log.h"
#include <cstdarg>
#include <cstdio>

// Global instance
RemoteLogger remoteLog;

RemoteLogger::RemoteLogger()
    : head_(0)
    , count_(0)
    , remote_min_level_(LogLevel::INFO)
    , serial_enabled_(true)
    , broadcast_callback_(nullptr)
    , mutex_(nullptr) {
}

void RemoteLogger::begin() {
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        Serial.println("[RemoteLog] Warning: Failed to create mutex");
    }
    info("Remote logger initialized");
}

void RemoteLogger::debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    logImpl(LogLevel::DEBUG, format, args);
    va_end(args);
}

void RemoteLogger::info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    logImpl(LogLevel::INFO, format, args);
    va_end(args);
}

void RemoteLogger::warn(const char* format, ...) {
    va_list args;
    va_start(args, format);
    logImpl(LogLevel::WARN, format, args);
    va_end(args);
}

void RemoteLogger::error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    logImpl(LogLevel::ERROR, format, args);
    va_end(args);
}

void RemoteLogger::log(LogLevel level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logImpl(level, format, args);
    va_end(args);
}

void RemoteLogger::logImpl(LogLevel level, const char* format, va_list args) {
    LogEntry entry;
    entry.timestamp = millis();
    entry.level = level;

    // Format the message
    vsnprintf(entry.message, sizeof(entry.message), format, args);

    // Always output to Serial if enabled
    if (serial_enabled_) {
        Serial.printf("[%s] %s\n", levelToString(level), entry.message);
    }

    // Add to ring buffer
    addEntry(entry);

    // Broadcast to WebSocket clients if level meets threshold
    if (level >= remote_min_level_ && broadcast_callback_) {
        broadcast_callback_(entry);
    }
}

void RemoteLogger::addEntry(const LogEntry& entry) {
    if (mutex_ != nullptr) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }

    buffer_[head_] = entry;
    head_ = (head_ + 1) % LOG_BUFFER_SIZE;
    if (count_ < LOG_BUFFER_SIZE) {
        count_++;
    }

    if (mutex_ != nullptr) {
        xSemaphoreGive(mutex_);
    }
}

size_t RemoteLogger::getRecentLogs(LogEntry* buffer, size_t max_entries) const {
    if (buffer == nullptr || max_entries == 0) {
        return 0;
    }

    if (mutex_ != nullptr) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }

    size_t to_copy = (max_entries < count_) ? max_entries : count_;

    // Calculate start position (oldest entry we'll copy)
    // Entries are stored oldest at tail, newest at head-1
    size_t start;
    if (count_ < LOG_BUFFER_SIZE) {
        // Buffer not full yet, start from 0
        start = (count_ > max_entries) ? (count_ - max_entries) : 0;
    } else {
        // Buffer is full, oldest is at head
        start = (head_ + LOG_BUFFER_SIZE - to_copy) % LOG_BUFFER_SIZE;
    }

    // Copy entries in chronological order (oldest first)
    for (size_t i = 0; i < to_copy; i++) {
        size_t idx = (start + i) % LOG_BUFFER_SIZE;
        buffer[i] = buffer_[idx];
    }

    if (mutex_ != nullptr) {
        xSemaphoreGive(mutex_);
    }

    return to_copy;
}

size_t RemoteLogger::getEntryCount() const {
    return count_;
}

void RemoteLogger::clear() {
    if (mutex_ != nullptr) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }

    head_ = 0;
    count_ = 0;

    if (mutex_ != nullptr) {
        xSemaphoreGive(mutex_);
    }
}

const char* RemoteLogger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}
