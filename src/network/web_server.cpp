#include "web_server.h"
#include "../config/config.h"
#include "../config/settings.h"
#include "../battery/battery_manager.h"
#include "../can/can_logger.h"
#include "../can/can_driver.h"
#include "../utils/remote_log.h"
#include <SPIFFS.h>

// Global instance
WebServer webServer;

// Persistent buffer pool for CAN log API - allocated once, reused forever
// Protected by mutex for thread safety
namespace {
    constexpr size_t CAN_LOG_BUFFER_SIZE = 200;
    CANMessage* persistent_can_buffer = nullptr;
    SemaphoreHandle_t can_buffer_mutex = nullptr;

    void initCANLogBuffer() {
        if (can_buffer_mutex == nullptr) {
            can_buffer_mutex = xSemaphoreCreateMutex();
        }
        // Allocate buffer once on first init (freed never - persistent for lifetime)
        if (persistent_can_buffer == nullptr) {
            persistent_can_buffer = new CANMessage[CAN_LOG_BUFFER_SIZE];
            if (persistent_can_buffer) {
                LOG_INFO("[WebServer] Persistent CAN buffer allocated: %d messages (%d bytes)",
                         CAN_LOG_BUFFER_SIZE, CAN_LOG_BUFFER_SIZE * sizeof(CANMessage));
            } else {
                LOG_ERROR("[WebServer] Failed to allocate persistent CAN buffer!");
            }
        }
    }

    bool acquireCANBuffer() {
        if (can_buffer_mutex == nullptr || persistent_can_buffer == nullptr) return false;
        return xSemaphoreTake(can_buffer_mutex, pdMS_TO_TICKS(100)) == pdTRUE;
    }

    void releaseCANBuffer() {
        if (can_buffer_mutex != nullptr) {
            xSemaphoreGive(can_buffer_mutex);
        }
    }
}

WebServer::WebServer(uint16_t port)
    : server_(port)
    , ws_("/ws")
    , port_(port)
    , settings_(nullptr)
    , batteries_(nullptr)
    , can_logger_(nullptr)
    , client_callback_(nullptr)
    , request_count_(0)
    , ws_messages_sent_(0)
    , last_ws_cleanup_(0)
    , can_batch_count_(0)
    , last_can_flush_(0) {
    portMUX_INITIALIZE(&can_batch_mux_);
}

WebServer::~WebServer() {
    stop();
}

bool WebServer::begin(SettingsManager* settings, BatteryManager* batteries, CANLogger* canLog,
                      Protocol::Loader* protocolLoader) {
    settings_ = settings;
    batteries_ = batteries;
    can_logger_ = canLog;
    protocol_loader_ = protocolLoader;

    LOG_INFO("[WebServer] Starting on port %d",port_);

    // Initialize static CAN buffer pool
    initCANLogBuffer();
    LOG_INFO("[WebServer] Static CAN buffer pool initialized (%d messages)", CAN_LOG_BUFFER_SIZE);

    // Initialize SPIFFS for static files
    if (!SPIFFS.begin(true)) {
        LOG_ERROR("[WebServer] SPIFFS mount failed, no static files will be served");
    } else {
        LOG_INFO("[WebServer] SPIFFS mounted successfully");
        LOG_INFO("[WebServer] Total: %d bytes, Used: %d bytes, Free: %d bytes",
                 SPIFFS.totalBytes(), SPIFFS.usedBytes(),
                 SPIFFS.totalBytes() - SPIFFS.usedBytes());
    }

    // Setup handlers
    setupWebSocket();
    setupStaticFiles();
    setupAPIEndpoints();

    // Start server
    server_.begin();
    LOG_INFO("[WebServer] Server started");

    return true;
}

void WebServer::stop() {
    ws_.closeAll();
    server_.end();
    LOG_INFO("[WebServer] Server stopped");
}

void WebServer::setupWebSocket() {
    ws_.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->onWSEvent(server, client, type, arg, data, len);
    });

    server_.addHandler(&ws_);
    LOG_INFO("[WebServer] WebSocket handler registered at /ws");
}

void WebServer::setupStaticFiles() {
    // Log SPIFFS contents for debugging
    LOG_INFO("[WebServer] Checking SPIFFS filesystem contents:");
    File root = SPIFFS.open("/");
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            LOG_INFO("[WebServer]   Found: %s (%d bytes)", file.name(), file.size());
            file = root.openNextFile();
        }
    } else {
        LOG_WARN("[WebServer] Failed to open SPIFFS root directory");
    }

    // Serve index.html at root
    server_.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        LOG_INFO("[WebServer] GET / from %s", request->client()->remoteIP().toString().c_str());

        if (SPIFFS.exists("/web/index.html")) {
            LOG_INFO("[WebServer] Serving /web/index.html");
            AsyncWebServerResponse* response = request->beginResponse(SPIFFS, "/web/index.html", "text/html");
            response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
            request->send(response);
        } else if (SPIFFS.exists("/index.html")) {
            LOG_INFO("[WebServer] Serving /index.html");
            AsyncWebServerResponse* response = request->beginResponse(SPIFFS, "/index.html", "text/html");
            response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
            request->send(response);
        } else {
            LOG_WARN("[WebServer] No index.html found, serving fallback page");
            // Fallback inline minimal page
            String html = "<!DOCTYPE html><html><head><title>eBike Monitor</title></head>"
                         "<body><h1>eBike Battery Monitor</h1>"
                         "<p>Web interface files not found. Upload filesystem with: pio run --target uploadfs</p>"
                         "<h2>API Endpoints</h2>"
                         "<ul>"
                         "<li><a href='/api/status'>/api/status</a></li>"
                         "<li><a href='/api/batteries'>/api/batteries</a></li>"
                         "<li><a href='/api/canlog'>/api/canlog</a></li>"
                         "<li><a href='/api/config'>/api/config</a></li>"
                         "<li><a href='/logs'>/logs</a> - Live log viewer</li>"
                         "</ul></body></html>";
            request->send(200, "text/html", html);
        }
    });

    // Serve embedded log viewer page
    server_.on("/logs", HTTP_GET, [](AsyncWebServerRequest* request) {
        String html = R"rawliteral(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>eBike Monitor - Logs</title>
    <style>
        * { box-sizing: border-box; }
        body { font-family: monospace; margin: 0; padding: 10px; background: #1a1a1a; color: #eee; }
        h1 { margin: 0 0 10px 0; font-size: 1.2em; }
        .controls { margin-bottom: 10px; }
        .controls button { margin-right: 5px; padding: 5px 10px; cursor: pointer; }
        .controls select { padding: 5px; }
        #status { padding: 5px 10px; border-radius: 3px; display: inline-block; margin-left: 10px; }
        #status.connected { background: #2a5; }
        #status.disconnected { background: #a33; }
        #log {
            background: #111;
            padding: 10px;
            height: calc(100vh - 100px);
            overflow-y: auto;
            border: 1px solid #333;
            font-size: 13px;
            line-height: 1.4;
        }
        .entry { margin: 2px 0; }
        .ts { color: #888; }
        .DEBUG { color: #888; }
        .INFO { color: #6cf; }
        .WARN { color: #fc6; }
        .ERROR { color: #f66; font-weight: bold; }
    </style>
</head>
<body>
    <h1>eBike Monitor - Live Logs</h1>
    <div class="controls">
        <button onclick="clearLog()">Clear</button>
        <button onclick="toggleScroll()">Auto-scroll: <span id="scrollState">ON</span></button>
        <select id="levelFilter" onchange="applyFilter()">
            <option value="DEBUG">Show All</option>
            <option value="INFO" selected>INFO+</option>
            <option value="WARN">WARN+</option>
            <option value="ERROR">ERROR only</option>
        </select>
        <span id="status" class="disconnected">Disconnected</span>
    </div>
    <div id="log"></div>
    <script>
        const logEl = document.getElementById('log');
        const statusEl = document.getElementById('status');
        const levels = ['DEBUG', 'INFO', 'WARN', 'ERROR'];
        let autoScroll = true;
        let minLevel = 'INFO';
        let ws;

        function connect() {
            ws = new WebSocket('ws://' + location.host + '/ws');
            ws.onopen = () => {
                statusEl.textContent = 'Connected';
                statusEl.className = 'connected';
            };
            ws.onclose = () => {
                statusEl.textContent = 'Disconnected';
                statusEl.className = 'disconnected';
                setTimeout(connect, 2000);
            };
            ws.onmessage = (e) => {
                const msg = JSON.parse(e.data);
                if (msg.type === 'log') {
                    addEntry(msg);
                } else if (msg.type === 'log_history') {
                    msg.logs.forEach(addEntry);
                }
            };
        }

        function addEntry(log) {
            if (levels.indexOf(log.level) < levels.indexOf(minLevel)) return;
            const div = document.createElement('div');
            div.className = 'entry';
            const ts = new Date(log.ts).toLocaleTimeString() || formatMs(log.ts);
            div.innerHTML = '<span class="ts">' + ts + '</span> <span class="' + log.level + '">[' + log.level + ']</span> ' + escapeHtml(log.msg);
            logEl.appendChild(div);
            if (autoScroll) logEl.scrollTop = logEl.scrollHeight;
        }

        function formatMs(ms) {
            const s = Math.floor(ms / 1000);
            const m = Math.floor(s / 60);
            const h = Math.floor(m / 60);
            return String(h).padStart(2,'0') + ':' + String(m%60).padStart(2,'0') + ':' + String(s%60).padStart(2,'0');
        }

        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }

        function clearLog() { logEl.innerHTML = ''; }

        function toggleScroll() {
            autoScroll = !autoScroll;
            document.getElementById('scrollState').textContent = autoScroll ? 'ON' : 'OFF';
        }

        function applyFilter() {
            minLevel = document.getElementById('levelFilter').value;
            clearLog();
        }

        connect();
    </script>
</body>
</html>)rawliteral";
        request->send(200, "text/html", html);
    });

    // Serve specific static files (JS, CSS, etc.) from /web directory
    server_.serveStatic("/app.js", SPIFFS, "/web/app.js");
    server_.serveStatic("/style.css", SPIFFS, "/web/style.css");

    // Serve all files from /static/ path for debugging
    server_.serveStatic("/static/", SPIFFS, "/");

    LOG_INFO("[WebServer] Static file handlers registered");
}

void WebServer::setupAPIEndpoints() {
    // GET /api/status - System and battery status
    server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request_count_++;
        handleGetStatus(request);
    });

    // GET /api/batteries - All battery data
    server_.on("/api/batteries", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request_count_++;
        handleGetBatteries(request);
    });

    // GET /api/battery/:id - Single battery data
    server_.on("^\\/api\\/battery\\/(\\d+)$", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request_count_++;
        uint8_t id = request->pathArg(0).toInt();
        handleGetBattery(request, id);
    });

    // GET /api/canlog - CAN message log
    server_.on("/api/canlog", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request_count_++;
        handleGetCANLog(request);
    });

    // GET /api/canlog/download - Download CAN log as CSV
    server_.on("/api/canlog/download", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request_count_++;
        handleDownloadCANLog(request);
    });

    // POST /api/canlog/clear - Clear CAN log
    server_.on("/api/canlog/clear", HTTP_POST, [this](AsyncWebServerRequest* request) {
        request_count_++;
        handleClearCANLog(request);
    });

    // GET /api/config - Current configuration
    server_.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request_count_++;
        handleGetConfig(request);
    });

    // POST /api/config - Update configuration
    server_.on("/api/config", HTTP_POST,
        [](AsyncWebServerRequest* request) {},  // Empty - handled in body callback
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                request_count_++;
                handlePostConfig(request, data, len);
            }
        }
    );

    // POST /api/config/battery/:id - Update single battery config
    server_.on("^\\/api\\/config\\/battery\\/(\\d+)$", HTTP_POST,
        [](AsyncWebServerRequest* request) {},  // Handled in body handler
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                request_count_++;
                uint8_t id = request->pathArg(0).toInt();
                handlePostBatteryConfig(request, id, data, len);
            }
        }
    );

    // POST /api/calibrate/:id - Calibrate current sensor
    server_.on("^\\/api\\/calibrate\\/(\\d+)$", HTTP_POST, [this](AsyncWebServerRequest* request) {
        request_count_++;
        uint8_t id = request->pathArg(0).toInt();
        handleCalibrate(request, id);
    });

    // POST /api/reset - Reboot device
    server_.on("/api/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        request_count_++;
        handleReset(request);
    });

    // GET /api/logs - Recent log messages
    server_.on("/api/logs", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request_count_++;
        handleGetLogs(request);
    });

    // GET /api/can/diagnostics - CAN bus diagnostics
    server_.on("/api/can/diagnostics", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request_count_++;
        handleGetCANDiagnostics(request);
    });

    // Handle 404
    server_.onNotFound([this](AsyncWebServerRequest* request) {
        handleNotFound(request);
    });

    LOG_INFO("[WebServer] API endpoints registered");
}

// API Handlers
void WebServer::handleGetStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;
    buildStatusJSON(doc.to<JsonObject>());
    sendJSON(request, doc);
}

void WebServer::handleGetBatteries(AsyncWebServerRequest* request) {
    JsonDocument doc;
    buildAllBatteriesJSON(doc.to<JsonObject>());
    sendJSON(request, doc);
}

void WebServer::handleGetBattery(AsyncWebServerRequest* request, uint8_t id) {
    if (batteries_ == nullptr || id >= MAX_BATTERY_MODULES) {
        sendError(request, 404, "Battery not found");
        return;
    }

    const BatteryModule* battery = batteries_->getBattery(id);
    if (battery == nullptr) {
        sendError(request, 404, "Battery not found");
        return;
    }

    JsonDocument doc;
    buildBatteryJSON(doc.to<JsonObject>(), id);
    sendJSON(request, doc);
}

void WebServer::handleGetCANLog(AsyncWebServerRequest* request) {
    if (can_logger_ == nullptr) {
        sendError(request, 500, "CAN logger not available");
        return;
    }

    // Try to acquire static buffer - fast path with no allocations
    if (!acquireCANBuffer()) {
        sendError(request, 503, "CAN log busy - try again");
        return;
    }

    // Parse parameters (avoid String operations)
    uint32_t filter_id = 0;
    bool has_filter = false;
    if (request->hasParam("filter")) {
        const char* filter_str = request->getParam("filter")->value().c_str();
        filter_id = strtoul(filter_str, nullptr, 0);
        has_filter = true;
    }

    size_t limit = CAN_LOG_BUFFER_SIZE;  // Use full buffer by default
    if (request->hasParam("limit")) {
        int l = request->getParam("limit")->value().toInt();
        if (l > 0 && l <= CAN_LOG_BUFFER_SIZE) limit = l;
    }

    // Use persistent buffer - allocated once, reused forever!
    size_t count = 0;
    bool success;

    if (has_filter) {
        success = can_logger_->getFilteredMessages(persistent_can_buffer, count, limit, filter_id);
    } else {
        success = can_logger_->getRecentMessages(persistent_can_buffer, count, limit);
    }

    if (!success) {
        releaseCANBuffer();
        sendError(request, 500, "Failed to retrieve messages");
        return;
    }

    // Use AsyncResponseStream for efficient streaming
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->addHeader("Access-Control-Allow-Origin", "*");

    // Pre-allocate write buffer on stack for batching writes
    char write_buffer[512];
    size_t write_pos = 0;

    // Helper to flush write buffer
    auto flush = [&]() {
        if (write_pos > 0) {
            response->write((uint8_t*)write_buffer, write_pos);
            write_pos = 0;
        }
    };

    // Helper to append to write buffer with auto-flush
    auto append = [&](const char* str, size_t len) {
        if (write_pos + len >= sizeof(write_buffer)) {
            flush();
        }
        if (len < sizeof(write_buffer)) {
            memcpy(write_buffer + write_pos, str, len);
            write_pos += len;
        } else {
            // String too large, write directly
            flush();
            response->write((uint8_t*)str, len);
        }
    };

    // Start JSON header
    const char* header = "{\"messages\":[";
    append(header, strlen(header));

    // Stream messages with zero-copy approach
    for (size_t i = 0; i < count; i++) {
        const CANMessage& msg = persistent_can_buffer[i];

        // Build JSON on stack
        char json_buf[128];
        char hex_data[17];

        // Format hex data efficiently
        char* hex_ptr = hex_data;
        for (int j = 0; j < msg.dlc && j < 8; j++) {
            uint8_t byte = msg.data[j];
            *hex_ptr++ = "0123456789ABCDEF"[byte >> 4];
            *hex_ptr++ = "0123456789ABCDEF"[byte & 0x0F];
        }
        *hex_ptr = '\0';

        // Format JSON message
        int len = snprintf(json_buf, sizeof(json_buf),
            "%s{\"id\":\"0x%03X\",\"dlc\":%u,\"data\":\"%s\",\"timestamp\":%lu,\"extended\":%s}",
            (i > 0 ? "," : ""),
            msg.id, msg.dlc, hex_data, msg.timestamp,
            msg.extended ? "true" : "false");

        if (len > 0 && len < (int)sizeof(json_buf)) {
            append(json_buf, len);
        }

        // Yield every 100 messages to prevent watchdog
        if (i % 100 == 0) {
            flush();
            yield();
        }
    }

    // Write footer
    char footer[128];
    int footer_len = snprintf(footer, sizeof(footer),
        "],\"count\":%u,\"total_logged\":%u,\"dropped\":%u}",
        count,
        can_logger_->getMessageCount(),
        can_logger_->getDroppedCount());

    if (footer_len > 0 && footer_len < (int)sizeof(footer)) {
        append(footer, footer_len);
    }

    // Final flush
    flush();

    // Release buffer ASAP
    releaseCANBuffer();

    // Send response
    request->send(response);
}

void WebServer::handleDownloadCANLog(AsyncWebServerRequest* request) {
    if (can_logger_ == nullptr) {
        sendError(request, 500, "CAN logger not available");
        return;
    }

    LOG_INFO("[WebServer] CAN log download requested");
    LOG_INFO("[WebServer] Total logged messages: %u, Dropped: %u",
             can_logger_->getMessageCount(), can_logger_->getDroppedCount());

    // Flush any pending messages to file first
    can_logger_->flush();

    // Check if file exists and get size
    if (!SPIFFS.exists("/canlog.csv")) {
        LOG_ERROR("[WebServer] CAN log file /canlog.csv not found on SPIFFS");
        LOG_ERROR("[WebServer] Make sure can_log_enabled is true in settings");
        sendError(request, 404, "CAN log file not found - check if CAN logging is enabled");
        return;
    }

    File file = SPIFFS.open("/canlog.csv", "r");
    if (!file) {
        LOG_ERROR("[WebServer] Failed to open /canlog.csv for reading");
        sendError(request, 500, "Failed to open CAN log file");
        return;
    }

    size_t fileSize = file.size();
    file.close();

    LOG_INFO("[WebServer] Serving CAN log file: %d bytes from SPIFFS", fileSize);

    // Serve the SPIFFS file directly
    AsyncWebServerResponse* response = request->beginResponse(SPIFFS, "/canlog.csv", "text/csv", true);
    response->addHeader("Content-Disposition", "attachment; filename=\"canlog.csv\"");
    request->send(response);
}

void WebServer::handleClearCANLog(AsyncWebServerRequest* request) {
    if (can_logger_ != nullptr) {
        can_logger_->clear();
    }

    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "CAN log cleared";
    sendJSON(request, doc);
}

void WebServer::handleGetConfig(AsyncWebServerRequest* request) {
    JsonDocument doc;
    buildConfigJSON(doc.to<JsonObject>());
    sendJSON(request, doc);
}

void WebServer::handlePostConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (settings_ == nullptr) {
        sendError(request, 500, "Settings not available");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        sendError(request, 400, "Invalid JSON");
        return;
    }

    Settings& settings = settings_->getSettings();

    // Update settings from JSON (using ArduinoJson v7 API)
    if (!doc["wifi_ssid"].isNull()) {
        strlcpy(settings.wifi_ssid, doc["wifi_ssid"] | "", sizeof(settings.wifi_ssid));
    }
    if (!doc["wifi_password"].isNull()) {
        strlcpy(settings.wifi_password, doc["wifi_password"] | "", sizeof(settings.wifi_password));
    }
    if (!doc["mqtt_enabled"].isNull()) {
        settings.mqtt_enabled = doc["mqtt_enabled"].as<bool>();
    }
    if (!doc["mqtt_broker"].isNull()) {
        strlcpy(settings.mqtt_broker, doc["mqtt_broker"] | "", sizeof(settings.mqtt_broker));
    }
    if (!doc["mqtt_port"].isNull()) {
        settings.mqtt_port = doc["mqtt_port"] | MQTT_DEFAULT_PORT;
    }
    if (!doc["mqtt_topic_prefix"].isNull()) {
        strlcpy(settings.mqtt_topic_prefix, doc["mqtt_topic_prefix"] | MQTT_TOPIC_PREFIX, sizeof(settings.mqtt_topic_prefix));
    }
    if (!doc["mqtt_username"].isNull()) {
        strlcpy(settings.mqtt_username, doc["mqtt_username"] | "", sizeof(settings.mqtt_username));
    }
    if (!doc["mqtt_password"].isNull()) {
        strlcpy(settings.mqtt_password, doc["mqtt_password"] | "", sizeof(settings.mqtt_password));
    }
    if (!doc["publish_interval_ms"].isNull()) {
        settings.publish_interval_ms = doc["publish_interval_ms"] | DEFAULT_PUBLISH_INTERVAL_MS;
    }
    if (!doc["sample_interval_ms"].isNull()) {
        settings.sample_interval_ms = doc["sample_interval_ms"] | DEFAULT_SAMPLE_INTERVAL_MS;
    }
    if (!doc["web_refresh_ms"].isNull()) {
        settings.web_refresh_ms = doc["web_refresh_ms"] | DEFAULT_WEB_REFRESH_MS;
    }
    if (!doc["can_bitrate"].isNull()) {
        settings.can_bitrate = doc["can_bitrate"] | CAN_BITRATE;
    }
    if (!doc["can_log_enabled"].isNull()) {
        settings.can_log_enabled = doc["can_log_enabled"].as<bool>();
    }
    if (!doc["mqtt_canmsg_enabled"].isNull()) {
        settings.mqtt_canmsg_enabled = doc["mqtt_canmsg_enabled"].as<bool>();
    }
    if (!doc["num_batteries"].isNull()) {
        settings.num_batteries = constrain(doc["num_batteries"] | 1, 1, MAX_BATTERY_MODULES);
    }

    // Save to NVS
    if (settings_->save()) {
        JsonDocument resp;
        resp["success"] = true;
        resp["message"] = "Configuration saved";
        sendJSON(request, resp);
    } else {
        LOG_ERROR("[Config] Failed to save configuration to NVS");
        sendError(request, 500, "Failed to save configuration");
    }
}

void WebServer::handlePostBatteryConfig(AsyncWebServerRequest* request, uint8_t id, uint8_t* data, size_t len) {
    if (settings_ == nullptr || id >= MAX_BATTERY_MODULES) {
        sendError(request, 404, "Battery not found");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        sendError(request, 400, "Invalid JSON");
        return;
    }

    BatteryConfig config = settings_->getSettings().batteries[id];

    if (!doc["enabled"].isNull()) {
        config.enabled = doc["enabled"] | true;
    }
    if (!doc["name"].isNull()) {
        strlcpy(config.name, doc["name"] | "", sizeof(config.name));
    }
    if (!doc["current_cal_offset"].isNull()) {
        config.current_cal_offset = doc["current_cal_offset"] | ACS712_ZERO_CURRENT_MV;
    }
    if (!doc["current_cal_scale"].isNull()) {
        config.current_cal_scale = doc["current_cal_scale"] | ACS712_20A_SENSITIVITY;
    }
    if (!doc["voltage_cal_scale"].isNull()) {
        config.voltage_cal_scale = doc["voltage_cal_scale"] | VOLTAGE_DIVIDER_RATIO;
    }
    if (!doc["can_base_id"].isNull()) {
        config.can_base_id = doc["can_base_id"] | 0;
    }

    if (settings_->updateBatteryConfig(id, config)) {
        JsonDocument resp;
        resp["success"] = true;
        resp["message"] = "Battery configuration saved";
        sendJSON(request, resp);
    } else {
        sendError(request, 500, "Failed to save battery configuration");
    }
}

void WebServer::handleCalibrate(AsyncWebServerRequest* request, uint8_t id) {
    if (id >= MAX_BATTERY_MODULES) {
        sendError(request, 404, "Battery not found");
        return;
    }

    // TODO: Trigger actual calibration of current sensor
    // For now, just acknowledge the request

    JsonDocument doc;
    doc["success"] = true;
    doc["battery_id"] = id;
    doc["message"] = "Calibration started";
    sendJSON(request, doc);
}

void WebServer::handleReset(AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "Rebooting...";
    sendJSON(request, doc);

    // Schedule reboot after response is sent
    delay(500);
    ESP.restart();
}

void WebServer::handleGetLogs(AsyncWebServerRequest* request) {
    // Limit parameter
    size_t limit = LOG_BUFFER_SIZE;
    if (request->hasParam("limit")) {
        int l = request->getParam("limit")->value().toInt();
        if (l > 0 && l <= LOG_BUFFER_SIZE) limit = l;
    }

    // Get recent logs
    LogEntry* logs = new LogEntry[limit];
    size_t count = remoteLog.getRecentLogs(logs, limit);

    JsonDocument doc;
    JsonArray arr = doc["logs"].to<JsonArray>();

    for (size_t i = 0; i < count; i++) {
        JsonObject entry = arr.add<JsonObject>();
        entry["ts"] = logs[i].timestamp;
        entry["level"] = RemoteLogger::levelToString(logs[i].level);
        entry["msg"] = logs[i].message;
    }

    doc["count"] = count;
    doc["buffer_size"] = LOG_BUFFER_SIZE;

    delete[] logs;
    sendJSON(request, doc);
}

void WebServer::handleGetCANDiagnostics(AsyncWebServerRequest* request) {
    char buffer[1024];

    if (canDriver.getDiagnostics(buffer, sizeof(buffer))) {
        // Return as plain text for readability
        AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", buffer);
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    } else {
        sendError(request, 500, "Failed to get CAN diagnostics");
    }
}

void WebServer::handleNotFound(AsyncWebServerRequest* request) {
    sendError(request, 404, "Not found");
}

// WebSocket handlers
void WebServer::onWSEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            {
                size_t free_heap = ESP.getFreeHeap();
                LOG_INFO("[WebSocket] Client #%u connected from %s (heap: %u bytes)",
                         client->id(), client->remoteIP().toString().c_str(), free_heap);

                // Limit maximum clients to prevent memory exhaustion
                if (ws_.count() > 5) {
                    LOG_ERROR("[WebSocket] Rejecting client - too many connections (%u active)", ws_.count());
                    client->text("{\"error\":\"Server full, max 5 clients\"}");
                    client->close();
                    return;
                }

                // Check if we have enough memory for this client
                if (free_heap < 20000) {
                    LOG_ERROR("[WebSocket] Rejecting client - insufficient heap (%u bytes)", free_heap);
                    client->text("{\"error\":\"Server low on memory\"}");
                    client->close();
                    return;
                }

                if (client_callback_) {
                    client_callback_(client->id(), true);
                }
                // Send initial status to new client
                {
                    JsonDocument doc;
                    buildStatusJSON(doc.to<JsonObject>());
                    String json;
                    serializeJson(doc, json);
                    client->text(json);
                }
                // Send log history to new client
                sendLogHistory(client);
            }
            break;

        case WS_EVT_DISCONNECT:
            LOG_DEBUG("[WebSocket] Client #%u disconnected",client->id());
            if (client_callback_) {
                client_callback_(client->id(), false);
            }
            break;

        case WS_EVT_DATA:
            // Handle incoming WebSocket data (commands from client)
            {
                AwsFrameInfo* info = (AwsFrameInfo*)arg;
                if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                    data[len] = 0;  // Null-terminate
                    LOG_DEBUG("[WebSocket] Received: %s",(char*)data);
                    // Could handle client commands here
                }
            }
            break;

        case WS_EVT_PONG:
        case WS_EVT_PING:
        case WS_EVT_ERROR:
            break;
    }
}

// Broadcasting
void WebServer::broadcastBatteryUpdate() {
    if (ws_.count() == 0) return;

    JsonDocument doc;
    doc["type"] = "battery_update";
    buildAllBatteriesJSON(doc["data"].to<JsonObject>());

    String json;
    serializeJson(doc, json);
    ws_.textAll(json);
    ws_messages_sent_++;
}

void WebServer::broadcastCANMessage(uint32_t id, uint8_t dlc, const uint8_t* data) {
    if (ws_.count() == 0) return;

    // Buffer the message for batch sending (flushed in loop() every 100ms)
    // This prevents WebSocket queue overflow when many CAN messages arrive
    // in a short burst, which causes the library to disconnect clients.
    portENTER_CRITICAL(&can_batch_mux_);
    if (can_batch_count_ < CAN_BATCH_MAX) {
        CANBatchEntry& entry = can_batch_[can_batch_count_++];
        entry.id = id;
        entry.dlc = dlc > 8 ? 8 : dlc;
        memcpy(entry.data, data, entry.dlc);
        entry.timestamp = millis();
    }
    portEXIT_CRITICAL(&can_batch_mux_);
}

void WebServer::flushCANBatch() {
    if (can_batch_count_ == 0 || ws_.count() == 0) return;

    if (ESP.getFreeHeap() < 10000) {
        // Drop batch under low memory
        portENTER_CRITICAL(&can_batch_mux_);
        can_batch_count_ = 0;
        portEXIT_CRITICAL(&can_batch_mux_);
        return;
    }

    // Copy batch under lock, then release immediately
    CANBatchEntry local_batch[CAN_BATCH_MAX];
    size_t count;

    portENTER_CRITICAL(&can_batch_mux_);
    count = can_batch_count_;
    if (count > 0) {
        memcpy(local_batch, (const void*)can_batch_, count * sizeof(CANBatchEntry));
        can_batch_count_ = 0;
    }
    portEXIT_CRITICAL(&can_batch_mux_);

    if (count == 0) return;

    // Calculate exact buffer size
    // Format: [type:1=0x02][count:1][entries...]
    // Each entry: [id:4][dlc:1][data:0-8][timestamp:4]
    size_t buf_size = 2;
    for (size_t i = 0; i < count; i++) {
        buf_size += 4 + 1 + local_batch[i].dlc + 4;
    }

    AsyncWebSocketMessageBuffer* buffer = ws_.makeBuffer(buf_size);
    if (!buffer) return;

    uint8_t* buf = buffer->get();
    size_t offset = 0;

    buf[offset++] = 0x02;  // Batch CAN message type
    buf[offset++] = (uint8_t)count;

    for (size_t i = 0; i < count; i++) {
        const CANBatchEntry& entry = local_batch[i];

        buf[offset++] = (entry.id >> 0) & 0xFF;
        buf[offset++] = (entry.id >> 8) & 0xFF;
        buf[offset++] = (entry.id >> 16) & 0xFF;
        buf[offset++] = (entry.id >> 24) & 0xFF;

        buf[offset++] = entry.dlc;
        memcpy(buf + offset, entry.data, entry.dlc);
        offset += entry.dlc;

        buf[offset++] = (entry.timestamp >> 0) & 0xFF;
        buf[offset++] = (entry.timestamp >> 8) & 0xFF;
        buf[offset++] = (entry.timestamp >> 16) & 0xFF;
        buf[offset++] = (entry.timestamp >> 24) & 0xFF;
    }

    ws_.binaryAll(buffer);
    ws_messages_sent_++;
}

void WebServer::broadcastSystemStatus() {
    if (ws_.count() == 0) return;

    JsonDocument doc;
    doc["type"] = "system_status";
    buildSystemJSON(doc["data"].to<JsonObject>());

    String json;
    serializeJson(doc, json);
    ws_.textAll(json);
    ws_messages_sent_++;
}

void WebServer::broadcastText(const char* message) {
    if (ws_.count() == 0) return;

    ws_.textAll(message);
    ws_messages_sent_++;
}

void WebServer::broadcastLog(const LogEntry& entry) {
    if (ws_.count() == 0) return;

    JsonDocument doc;
    doc["type"] = "log";
    doc["ts"] = entry.timestamp;
    doc["level"] = RemoteLogger::levelToString(entry.level);
    doc["msg"] = entry.message;

    String json;
    serializeJson(doc, json);
    ws_.textAll(json);
    ws_messages_sent_++;
}

void WebServer::sendLogHistory(AsyncWebSocketClient* client) {
    if (client == nullptr) return;

    // Check heap before allocating - need at least 15KB free for log history
    if (ESP.getFreeHeap() < 15000) {
        LOG_WARN("[WebSocket] Low heap (%u bytes), skipping log history", ESP.getFreeHeap());
        return;
    }

    // Limit the number of log entries to prevent memory allocation failures
    // Each entry can have 128+ bytes of message, so keep this small
    constexpr size_t MAX_HISTORY_ENTRIES = 10;  // Reduced from 20

    LogEntry* logs = new (std::nothrow) LogEntry[MAX_HISTORY_ENTRIES];
    if (logs == nullptr) {
        LOG_WARN("[WebSocket] Failed to allocate log history buffer");
        return;
    }

    size_t count = remoteLog.getRecentLogs(logs, MAX_HISTORY_ENTRIES);

    if (count > 0) {
        // Send as a batch message
        JsonDocument doc;
        doc["type"] = "log_history";
        JsonArray arr = doc["logs"].to<JsonArray>();

        for (size_t i = 0; i < count; i++) {
            JsonObject entry = arr.add<JsonObject>();
            entry["ts"] = logs[i].timestamp;
            entry["level"] = RemoteLogger::levelToString(logs[i].level);
            entry["msg"] = logs[i].message;
        }

        String json;
        serializeJson(doc, json);

        // Check if the message is too large before sending
        if (json.length() < 4096) {  // Limit to 4KB
            client->text(json);
        } else {
            LOG_WARN("[WebSocket] Log history too large (%d bytes), skipping", json.length());
        }
    }

    delete[] logs;
}

uint32_t WebServer::getWSClientCount() const {
    return ws_.count();
}

void WebServer::loop() {
    uint32_t now = millis();

    // Flush batched CAN messages every 100ms as a single WebSocket frame
    if (now - last_can_flush_ >= 100) {
        flushCANBatch();
        last_can_flush_ = now;
    }

    // Perform WebSocket cleanup and ping every 10 seconds
    if (now - last_ws_cleanup_ > 10000) {
        last_ws_cleanup_ = now;

        // Clean up dead connections
        ws_.cleanupClients();

        // Send ping to all clients to keep connections alive
        if (ws_.count() > 0) {
            ws_.pingAll();
            LOG_DEBUG("[WebSocket] Sent ping to %u clients", ws_.count());
        }
    }
}

// JSON builders
void WebServer::buildStatusJSON(JsonObject obj) {
    buildSystemJSON(obj["system"].to<JsonObject>());
    buildAllBatteriesJSON(obj["batteries"].to<JsonObject>());
}

void WebServer::buildBatteryJSON(JsonObject obj, uint8_t id) {
    if (batteries_ == nullptr || id >= MAX_BATTERY_MODULES) return;

    const BatteryModule* battery = batteries_->getBattery(id);
    if (battery == nullptr) return;

    obj["id"] = id;
    obj["name"] = battery->getName();
    obj["enabled"] = battery->isEnabled();
    obj["voltage"] = battery->getVoltage();
    obj["current"] = battery->getCurrent();
    obj["power"] = battery->getPower();
    obj["soc"] = battery->getSOC();
    obj["temp1"] = battery->getTemp1();
    obj["temp2"] = battery->getTemp2();
    obj["status_flags"] = battery->getStatusFlags();
    obj["pack_identifier"] = battery->getPackIdentifier();
    obj["has_can_data"] = battery->hasCANData();
    obj["has_error"] = battery->hasError();
    obj["last_update"] = battery->getLastUpdate();
    obj["data_fresh"] = battery->isDataFresh(5000);
}

void WebServer::buildAllBatteriesJSON(JsonObject obj) {
    if (batteries_ == nullptr) return;

    JsonArray arr = obj["batteries"].to<JsonArray>();
    float total_power = 0;
    float total_current = 0;

    for (uint8_t i = 0; i < batteries_->getActiveBatteryCount(); i++) {
        const BatteryModule* battery = batteries_->getBattery(i);
        if (battery != nullptr && battery->isEnabled()) {
            JsonObject battObj = arr.add<JsonObject>();
            battObj["id"] = i;
            battObj["name"] = battery->getName();
            battObj["voltage"] = battery->getVoltage();
            battObj["current"] = battery->getCurrent();
            battObj["power"] = battery->getPower();
            battObj["soc"] = battery->getSOC();
            battObj["temp1"] = battery->getTemp1();
            battObj["temp2"] = battery->getTemp2();
            battObj["has_error"] = battery->hasError();

            total_power += battery->getPower();
            total_current += battery->getCurrent();
        }
    }

    obj["total_power"] = total_power;
    obj["total_current"] = total_current;
    obj["average_voltage"] = batteries_->getAverageVoltage();
    obj["timestamp"] = millis();
}

void WebServer::buildConfigJSON(JsonObject obj) {
    if (settings_ == nullptr) return;

    const Settings& settings = settings_->getSettings();

    // Network (hide password for security)
    obj["wifi_ssid"] = settings.wifi_ssid;
    obj["wifi_configured"] = strlen(settings.wifi_password) > 0;
    obj["mqtt_enabled"] = settings.mqtt_enabled;
    obj["mqtt_broker"] = settings.mqtt_broker;
    obj["mqtt_port"] = settings.mqtt_port;
    obj["mqtt_topic_prefix"] = settings.mqtt_topic_prefix;
    obj["mqtt_username"] = settings.mqtt_username;

    // Timing
    obj["publish_interval_ms"] = settings.publish_interval_ms;
    obj["sample_interval_ms"] = settings.sample_interval_ms;
    obj["web_refresh_ms"] = settings.web_refresh_ms;

    // CAN
    obj["can_bitrate"] = settings.can_bitrate;
    obj["can_log_enabled"] = settings.can_log_enabled;
    obj["mqtt_canmsg_enabled"] = settings.mqtt_canmsg_enabled;

    // Batteries
    obj["num_batteries"] = settings.num_batteries;

    JsonArray batteries = obj["batteries"].to<JsonArray>();
    for (uint8_t i = 0; i < MAX_BATTERY_MODULES; i++) {
        JsonObject batt = batteries.add<JsonObject>();
        batt["id"] = i;
        batt["enabled"] = settings.batteries[i].enabled;
        batt["name"] = settings.batteries[i].name;
        batt["current_cal_offset"] = settings.batteries[i].current_cal_offset;
        batt["current_cal_scale"] = settings.batteries[i].current_cal_scale;
        batt["voltage_cal_scale"] = settings.batteries[i].voltage_cal_scale;
        batt["can_base_id"] = settings.batteries[i].can_base_id;
    }
}

void WebServer::buildSystemJSON(JsonObject obj) {
    obj["uptime_ms"] = millis();
    obj["free_heap"] = ESP.getFreeHeap();
    obj["min_free_heap"] = ESP.getMinFreeHeap();
    obj["chip_model"] = ESP.getChipModel();
    obj["chip_revision"] = ESP.getChipRevision();
    obj["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
    obj["flash_size"] = ESP.getFlashChipSize();
    obj["sdk_version"] = ESP.getSdkVersion();

    // WiFi info
    obj["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    if (WiFi.status() == WL_CONNECTED) {
        obj["wifi_ssid"] = WiFi.SSID();
        obj["wifi_rssi"] = WiFi.RSSI();
        obj["wifi_ip"] = WiFi.localIP().toString();
    }

    // CAN bus stats
    if (can_logger_ != nullptr) {
        obj["can_message_count"] = can_logger_->getMessageCount();
        obj["can_dropped_count"] = can_logger_->getDroppedCount();
    } else {
        obj["can_message_count"] = 0;
        obj["can_dropped_count"] = 0;
    }

    // Web server stats
    obj["http_requests"] = request_count_;
    obj["ws_clients"] = ws_.count();
    obj["ws_messages_sent"] = ws_messages_sent_;
}

// Utility functions
void WebServer::sendJSON(AsyncWebServerRequest* request, JsonDocument& doc, int code) {
    String json;
    serializeJson(doc, json);

    AsyncWebServerResponse* response = request->beginResponse(code, "application/json", json);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

void WebServer::sendError(AsyncWebServerRequest* request, int code, const char* message) {
    JsonDocument doc;
    doc["error"] = true;
    doc["code"] = code;
    doc["message"] = message;
    sendJSON(request, doc, code);
}
