#include "can_logger.h"
#include "../config/config.h"

// Global instance
CANLogger canLogger;

CANLogger::CANLogger()
    : is_initialized(false),
      message_count(0),
      dropped_count(0),
      last_flush_time(0),
      auto_flush(true),
      flush_interval_ms(CAN_LOG_FLUSH_INTERVAL_MS),
      mutex_(nullptr) {
    memset(log_filename, 0, sizeof(log_filename));
}

bool CANLogger::begin(const char* log_file) {
    if (is_initialized) {
        Serial.println("CANLogger: Already initialized");
        return true;
    }

    Serial.println("CANLogger: Initializing...");

    // Create mutex for thread safety
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        Serial.println("CANLogger: Failed to create mutex");
        return false;
    }

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {  // true = format on fail
        Serial.println("CANLogger: Failed to mount SPIFFS");
        return false;
    }

    // Store log filename
    strlcpy(log_filename, log_file, sizeof(log_filename));

    // Check available space
    size_t total = SPIFFS.totalBytes();
    size_t used = SPIFFS.usedBytes();
    Serial.printf("CANLogger: SPIFFS - Total: %d bytes, Used: %d bytes\n", total, used);

    // Check if log file exists and is empty
    bool file_exists = SPIFFS.exists(log_filename);
    bool write_header = false;

    if (file_exists) {
        File f = SPIFFS.open(log_filename, "r");
        if (f && f.size() == 0) {
            write_header = true;
        }
        if (f) f.close();
    } else {
        write_header = true;
    }

    // Create/open log file and write header if needed
    if (write_header) {
        if (!openLogFile("w")) {
            Serial.println("CANLogger: Failed to create log file");
            return false;
        }
        this->log_file.println("Timestamp,ID,DLC,Data,Extended,RTR");
        closeLogFile();
    }

    is_initialized = true;
    last_flush_time = millis();

    Serial.printf("CANLogger: Initialized, logging to %s\n", log_filename);
    return true;
}

void CANLogger::end() {
    if (!is_initialized) {
        return;
    }

    Serial.println("CANLogger: Shutting down...");

    // Flush any remaining messages
    flush();

    closeLogFile();
    SPIFFS.end();

    // Delete mutex
    if (mutex_ != nullptr) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }

    is_initialized = false;

    Serial.println("CANLogger: Shutdown complete");
}

bool CANLogger::logMessage(const CANMessage& msg) {
    if (!is_initialized) {
        return false;
    }

    // Add to in-memory buffer for web interface
    if (!memory_buffer.push(msg)) {
        // Memory buffer full, oldest message will be overwritten
    }

    // Add to write buffer
    if (!write_buffer.push(msg)) {
        dropped_count++;
        return false;
    }

    message_count++;

    // Check if we should auto-flush
    if (auto_flush && (millis() - last_flush_time) >= flush_interval_ms) {
        return flush();
    }

    return true;
}

bool CANLogger::flush() {
    if (!is_initialized || write_buffer.isEmpty()) {
        return true;
    }

    // Try to take mutex with timeout (don't block indefinitely)
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
        // Another task has the mutex, skip this flush
        return true;
    }

    // Open log file in append mode
    if (!openLogFile("a")) {
        Serial.println("CANLogger: Failed to open log file for flushing");
        xSemaphoreGive(mutex_);
        return false;
    }

    // Write all buffered messages
    CANMessage msg;
    size_t written = 0;

    while (write_buffer.pop(msg)) {
        if (writeMessageToFile(msg)) {
            written++;
        }
    }

    closeLogFile();
    last_flush_time = millis();

    xSemaphoreGive(mutex_);

    // Check if rotation is needed (outside mutex to avoid deadlock)
    checkAndRotate();

    if (written > 0) {
        Serial.printf("CANLogger: Flushed %d messages\n", written);
    }

    return true;
}

bool CANLogger::clear() {
    if (!is_initialized) {
        return false;
    }

    // Take mutex
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Serial.println("CANLogger: Failed to acquire mutex for clear");
        return false;
    }

    Serial.println("CANLogger: Clearing log file...");

    // Remove the file
    if (SPIFFS.exists(log_filename)) {
        SPIFFS.remove(log_filename);
    }

    // Recreate with header
    if (!openLogFile("w")) {
        xSemaphoreGive(mutex_);
        return false;
    }

    log_file.println("Timestamp,ID,DLC,Data,Extended,RTR");
    closeLogFile();

    // Clear in-memory buffers
    memory_buffer.clear();
    write_buffer.clear();

    message_count = 0;
    dropped_count = 0;

    xSemaphoreGive(mutex_);

    Serial.println("CANLogger: Log cleared");
    return true;
}

size_t CANLogger::getLogSize() {
    if (!is_initialized) {
        return 0;
    }

    // Take mutex with short timeout
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
        return 0;
    }

    size_t size = 0;
    if (SPIFFS.exists(log_filename)) {
        File f = SPIFFS.open(log_filename, "r");
        if (f) {
            size = f.size();
            f.close();
        }
    }

    xSemaphoreGive(mutex_);
    return size;
}

bool CANLogger::exportCSV(Stream& output) {
    if (!is_initialized) {
        return false;
    }

    flush();  // Flush pending messages first

    // Take mutex
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }

    if (!openLogFile("r")) {
        xSemaphoreGive(mutex_);
        return false;
    }

    // Copy file to output stream
    while (log_file.available()) {
        output.write(log_file.read());
    }

    closeLogFile();
    xSemaphoreGive(mutex_);
    return true;
}

bool CANLogger::exportFiltered(Stream& output, uint32_t filter_id) {
    if (!is_initialized) {
        return false;
    }

    flush();  // Flush pending messages first

    // Take mutex
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }

    if (!openLogFile("r")) {
        xSemaphoreGive(mutex_);
        return false;
    }

    // Write CSV header
    output.println("Timestamp,ID,DLC,Data,Extended,RTR");

    // Read and filter
    String line;
    bool first_line = true;

    while (log_file.available()) {
        line = log_file.readStringUntil('\n');

        // Skip header
        if (first_line) {
            first_line = false;
            continue;
        }

        // Parse ID from CSV (format: timestamp,ID,...)
        int comma_pos = line.indexOf(',');
        if (comma_pos < 0) continue;

        int second_comma = line.indexOf(',', comma_pos + 1);
        if (second_comma < 0) continue;

        String id_str = line.substring(comma_pos + 1, second_comma);
        uint32_t msg_id = strtoul(id_str.c_str(), nullptr, 16);

        // Filter by ID
        if (msg_id == filter_id) {
            output.println(line);
        }
    }

    closeLogFile();
    xSemaphoreGive(mutex_);
    return true;
}

bool CANLogger::getRecentMessages(CANMessage* buffer, size_t& count, size_t max_count) {
    count = 0;

    if (!is_initialized || buffer == nullptr) {
        return false;
    }

    // Copy from memory buffer
    memory_buffer.forEach([&](const CANMessage& msg) {
        if (count < max_count) {
            buffer[count++] = msg;
        }
    });

    return true;
}

bool CANLogger::getFilteredMessages(CANMessage* buffer, size_t& count, size_t max_count, uint32_t filter_id) {
    count = 0;

    if (!is_initialized || buffer == nullptr) {
        return false;
    }

    // Copy filtered messages from memory buffer
    memory_buffer.forEach([&](const CANMessage& msg) {
        if (count < max_count && msg.id == filter_id) {
            buffer[count++] = msg;
        }
    });

    return true;
}

bool CANLogger::openLogFile(const char* mode) {
    closeLogFile();  // Close if already open

    log_file = SPIFFS.open(log_filename, mode);
    if (!log_file) {
        Serial.printf("CANLogger: Failed to open %s in mode %s\n", log_filename, mode);
        return false;
    }

    return true;
}

void CANLogger::closeLogFile() {
    if (log_file) {
        log_file.close();
    }
}

bool CANLogger::writeMessageToFile(const CANMessage& msg) {
    if (!log_file) {
        return false;
    }

    char buffer[128];
    formatMessageCSV(msg, buffer, sizeof(buffer));

    log_file.println(buffer);
    return true;
}

bool CANLogger::checkAndRotate() {
    // Check SPIFFS usage
    size_t total = SPIFFS.totalBytes();
    size_t used = SPIFFS.usedBytes();
    uint8_t usage_percent = (used * 100) / total;

    if (usage_percent >= SPIFFS_ROTATION_PERCENT) {
        Serial.printf("CANLogger: SPIFFS usage at %d%%, rotating log...\n", usage_percent);

        // Simple rotation: delete oldest half of the log
        // A more sophisticated approach would keep recent messages

        // For now, just clear the log when it gets too big
        // In a production system, you'd want to archive or rotate properly
        clear();

        Serial.println("CANLogger: Log rotated (cleared)");
        return true;
    }

    return false;
}

void CANLogger::formatMessageCSV(const CANMessage& msg, char* buffer, size_t size) {
    // Format: Timestamp,ID,DLC,Data,Extended,RTR
    char data_str[25] = "";  // 8 bytes * 2 hex chars + spaces = 24 chars

    for (uint8_t i = 0; i < msg.dlc && i < 8; i++) {
        char byte_str[4];
        snprintf(byte_str, sizeof(byte_str), "%02X", msg.data[i]);
        strlcat(data_str, byte_str, sizeof(data_str));
        if (i < msg.dlc - 1) {
            strlcat(data_str, " ", sizeof(data_str));
        }
    }

    snprintf(buffer, size, "%u,0x%03X,%d,%s,%d,%d",
             msg.timestamp,
             msg.id,
             msg.dlc,
             data_str,
             msg.extended ? 1 : 0,
             msg.rtr ? 1 : 0);
}
