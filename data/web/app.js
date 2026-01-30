// eBike Battery Monitor - Web App
// Mobile-optimized with WebSocket support

class BatteryMonitor {
  constructor() {
    this.ws = null;
    this.reconnectInterval = null;
    this.config = {};
    this.batteries = [];

    // CAN monitor state
    this.canMonitor = {
      paused: false,
      messageCount: 0,
      maxMessages: 1000,
      filter: null,
    };

    this.init();
  }

  init() {
    this.setupEventListeners();
    this.loadConfig();
    this.connectWebSocket();
    this.startPeriodicUpdates();
  }

  setupEventListeners() {
    // Close WebSocket when page is hidden or closed
    document.addEventListener("visibilitychange", () => {
      if (document.hidden) {
        console.log("Page hidden, closing WebSocket");
        if (this.ws) {
          this.ws.close();
        }
        this.clearReconnectInterval();
      } else {
        console.log("Page visible, reconnecting WebSocket");
        this.connectWebSocket();
      }
    });

    // Close WebSocket on page unload
    window.addEventListener("beforeunload", () => {
      if (this.ws) {
        this.ws.close();
      }
      this.clearReconnectInterval();
    });

    // Settings button
    document.getElementById("settingsBtn").addEventListener("click", () => {
      this.showModal();
    });

    // Close settings
    document.getElementById("closeSettings").addEventListener("click", () => {
      this.hideModal();
    });

    // Click outside modal to close
    document.getElementById("settingsModal").addEventListener("click", (e) => {
      if (e.target.id === "settingsModal") {
        this.hideModal();
      }
    });

    // WiFi form
    document.getElementById("wifiForm").addEventListener("submit", (e) => {
      e.preventDefault();
      this.saveWiFiConfig();
    });

    // MQTT form
    document.getElementById("mqttForm").addEventListener("submit", (e) => {
      e.preventDefault();
      this.saveMQTTConfig();
    });

    // CAN Logging save button (using click instead of form submit)
    const saveCanBtn = document.getElementById("saveCanSettingsBtn");
    if (saveCanBtn) {
      saveCanBtn.addEventListener("click", () => {
        this.saveCANLoggingConfig();
      });
    }

    // Reboot button
    document.getElementById("rebootBtn").addEventListener("click", () => {
      if (confirm("Are you sure you want to reboot the device?")) {
        this.rebootDevice();
      }
    });

    // Clear WiFi button
    document.getElementById("clearWiFiBtn").addEventListener("click", () => {
      if (
        confirm(
          "This will clear WiFi settings and reboot. The device will start in AP mode. Continue?",
        )
      ) {
        this.clearWiFi();
      }
    });

    // CAN Monitor controls
    document.getElementById("canPauseBtn").addEventListener("click", () => {
      this.toggleCANMonitorPause();
    });

    document.getElementById("canClearBtn").addEventListener("click", () => {
      this.clearCANMonitor();
    });

    document.getElementById("canCopyBtn").addEventListener("click", () => {
      this.copyCANMonitor();
    });

    document.getElementById("canFilterInput").addEventListener("input", (e) => {
      this.setCANFilter(e.target.value.trim());
    });
  }

  // WebSocket Management
  connectWebSocket() {
    // Prevent multiple simultaneous connection attempts
    if (this.ws && (this.ws.readyState === WebSocket.CONNECTING || this.ws.readyState === WebSocket.OPEN)) {
      console.log("WebSocket already connecting or connected, skipping");
      return;
    }

    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const wsUrl = `${protocol}//${window.location.host}/ws`;

    console.log("Connecting to WebSocket:", wsUrl);

    try {
      // Close existing connection if any
      if (this.ws) {
        this.ws.close();
        this.ws = null;
      }

      this.ws = new WebSocket(wsUrl);

      // Set binary type to arraybuffer (not blob) for efficient binary handling
      this.ws.binaryType = 'arraybuffer';

      this.ws.onopen = () => {
        console.log("WebSocket connected");
        this.showToast("Connected", "success");
        this.clearReconnectInterval();
      };

      this.ws.onmessage = (event) => {
        // Debug: log the type of data received
        // console.log("WebSocket received data type:", typeof event.data, event.data.constructor.name);

        // Handle both binary and text messages
        if (event.data instanceof ArrayBuffer) {
          // console.log("Processing as ArrayBuffer, size:", event.data.byteLength);
          this.handleBinaryMessage(event.data);
        } else if (event.data instanceof Blob) {
          // console.log("Processing as Blob, size:", event.data.size);
          // Convert Blob to ArrayBuffer
          event.data.arrayBuffer().then((buffer) => {
            this.handleBinaryMessage(buffer);
          });
        } else {
          // console.log("Processing as text/JSON");
          // Text message (JSON)
          this.handleWebSocketMessage(event.data);
        }
      };

      this.ws.onerror = (error) => {
        console.error("WebSocket error:", error);
      };

      this.ws.onclose = () => {
        console.log("WebSocket disconnected");
        this.showToast("Disconnected - Reconnecting...", "warning");
        this.scheduleReconnect();
      };
    } catch (error) {
      console.error("Failed to create WebSocket:", error);
      this.scheduleReconnect();
    }
  }

  handleWebSocketMessage(data) {
    try {
      // Safety check: if this is somehow a Blob or ArrayBuffer, reject it
      if (data instanceof Blob || data instanceof ArrayBuffer) {
        console.error("Binary data incorrectly routed to JSON handler");
        return;
      }

      const message = JSON.parse(data);

      // Only log non-log messages to avoid spam (logs go to /logs page)
      if (message.type !== "log" && message.type !== "log_history") {
        console.log("WebSocket message:", message.type, message);
      }

      switch (message.type) {
        case "battery_update":
          this.updateBatteries(message.data);
          break;
        case "system_status":
          this.updateSystemStatus(message.data);
          break;
        case "can_message":
          this.handleCANMessage(message);
          break;
        case "log":
        case "log_history":
          // Ignore - these are for the /logs page
          break;
        default:
          // Handle initial status message (no type field)
          if (message.system) {
            this.updateSystemStatus(message.system);
          }
          if (message.batteries) {
            this.updateBatteries(message.batteries);
          }
      }
    } catch (error) {
      console.error("Error parsing WebSocket message:", error);
    }
  }

  handleBinaryMessage(buffer) {
    try {
      const view = new DataView(buffer);
      let offset = 0;

      if (buffer.byteLength < 2) {
        console.error("Binary message too short:", buffer.byteLength);
        return;
      }

      const type = view.getUint8(offset++);

      if (type === 0x01) {
        // Single CAN message (legacy format)
        if (buffer.byteLength < 10) return;

        const id = view.getUint32(offset, true);
        offset += 4;
        const dlc = view.getUint8(offset++);
        if (dlc > 8 || buffer.byteLength < offset + dlc + 4) return;

        const data = new Uint8Array(buffer, offset, dlc);
        offset += dlc;
        const timestamp = view.getUint32(offset, true);

        const idHex = "0x" + id.toString(16).toUpperCase().padStart(3, "0");
        const dataHex = Array.from(data)
          .map((b) => b.toString(16).toUpperCase().padStart(2, "0"))
          .join("");

        this.handleCANMessage({
          type: "can_message",
          id: idHex,
          dlc: dlc,
          data: dataHex,
          timestamp: timestamp,
        });
      } else if (type === 0x02) {
        // Batch CAN messages
        const count = view.getUint8(offset++);

        for (let i = 0; i < count; i++) {
          if (offset + 5 > buffer.byteLength) break;

          const id = view.getUint32(offset, true);
          offset += 4;
          const dlc = view.getUint8(offset++);
          if (dlc > 8 || offset + dlc + 4 > buffer.byteLength) break;

          const data = new Uint8Array(buffer, offset, dlc);
          offset += dlc;
          const timestamp = view.getUint32(offset, true);
          offset += 4;

          const idHex = "0x" + id.toString(16).toUpperCase().padStart(3, "0");
          const dataHex = Array.from(data)
            .map((b) => b.toString(16).toUpperCase().padStart(2, "0"))
            .join("");

          this.handleCANMessage({
            type: "can_message",
            id: idHex,
            dlc: dlc,
            data: dataHex,
            timestamp: timestamp,
          });
        }
      } else {
        console.warn("Unknown binary message type:", type);
      }
    } catch (error) {
      console.error("Error parsing binary message:", error);
    }
  }

  scheduleReconnect() {
    if (this.reconnectInterval) {
      console.log("Reconnect already scheduled");
      return;
    }

    console.log("Scheduling reconnect in 1 second...");
    this.reconnectInterval = setInterval(() => {
      console.log("Attempting to reconnect...");
      this.connectWebSocket();
    }, 1000);
  }

  clearReconnectInterval() {
    if (this.reconnectInterval) {
      console.log("Clearing reconnect interval");
      clearInterval(this.reconnectInterval);
      this.reconnectInterval = null;
    }
  }

  // UI Updates
  updateBatteries(data) {
    this.batteries = data.batteries || [];

    // Update summary
    document.getElementById("totalPower").textContent =
      (data.total_power || 0).toFixed(1) + " W";
    document.getElementById("totalCurrent").textContent =
      (data.total_current || 0).toFixed(2) + " A";
    document.getElementById("avgVoltage").textContent =
      (data.average_voltage || 0).toFixed(1) + " V";

    // Update or create battery cards
    const container = document.getElementById("batteriesContainer");

    if (this.batteries.length === 0) {
      container.innerHTML =
        '<div class="loading">No batteries configured</div>';
      return;
    }

    // Clear existing cards
    container.innerHTML = "";

    // Create cards for each battery
    this.batteries.forEach((battery) => {
      const card = this.createBatteryCard(battery);
      container.appendChild(card);
    });
  }

  createBatteryCard(battery) {
    const card = document.createElement("div");
    card.className = "battery-card";
    if (battery.has_error) {
      card.classList.add("error");
    }

    const statusClass = battery.has_error ? "error" : "ok";
    const statusText = battery.has_error ? "Error" : "OK";

    // Parse pack identifier if available
    let packInfoHTML = "";
    if (battery.pack_identifier) {
      const packInfo = this.parsePackIdentifier(battery.pack_identifier);
      if (packInfo) {
        packInfoHTML = `
                    <div class="battery-info">
                        <div class="info-item">
                            <span class="info-icon">📅</span>
                            <span class="info-text">${packInfo.date}</span>
                        </div>
                        <div class="info-item">
                            <span class="info-icon">🔢</span>
                            <span class="info-text">S/N: ${packInfo.serial}</span>
                        </div>
                    </div>
                `;
      }
    }

    card.innerHTML = `
            <div class="battery-header">
                <div class="battery-name">${this.escapeHtml(battery.name || `Battery ${battery.id}`)}</div>
                <div class="battery-status ${statusClass}">${statusText}</div>
            </div>
            ${packInfoHTML}
            <div class="battery-metrics">
                <div class="metric">
                    <span class="metric-label">Voltage</span>
                    <span class="metric-value">${(battery.voltage || 0).toFixed(1)} <span class="metric-unit">V</span></span>
                </div>
                <div class="metric">
                    <span class="metric-label">Current</span>
                    <span class="metric-value">${(battery.current || 0).toFixed(2)} <span class="metric-unit">A</span></span>
                </div>
                <div class="metric">
                    <span class="metric-label">Power</span>
                    <span class="metric-value">${(battery.power || 0).toFixed(1)} <span class="metric-unit">W</span></span>
                </div>
                <div class="metric">
                    <span class="metric-label">SOC</span>
                    <span class="metric-value">${battery.soc || 0} <span class="metric-unit">%</span></span>
                </div>
            </div>
        `;

    return card;
  }

  /**
   * Parse D-power pack identifier format: YYDDMMSSSS
   * @param {number} identifier - The 32-bit pack identifier value
   * @returns {object|null} - Parsed date and serial, or null if invalid
   */
  parsePackIdentifier(identifier) {
    if (!identifier || identifier <= 0) return null;

    try {
      // Convert to string and ensure it's the right length
      const idStr = identifier.toString();

      // Extract components using decimal division/modulo
      const year = Math.floor(identifier / 100000000) + 2000;
      const day = Math.floor(identifier / 1000000) % 100;
      const month = Math.floor(identifier / 10000) % 100;
      const serial = identifier % 10000;

      // Validate ranges
      if (year < 2000 || year > 2099) return null;
      if (month < 1 || month > 12) return null;
      if (day < 1 || day > 31) return null;

      // Format date nicely
      const monthNames = [
        "Jan",
        "Feb",
        "Mar",
        "Apr",
        "May",
        "Jun",
        "Jul",
        "Aug",
        "Sep",
        "Oct",
        "Nov",
        "Dec",
      ];
      const dateStr = `${monthNames[month - 1]} ${day}, ${year}`;

      return {
        date: dateStr,
        serial: serial.toString().padStart(4, "0"),
        year: year,
        month: month,
        day: day,
      };
    } catch (error) {
      console.error("Error parsing pack identifier:", error);
      return null;
    }
  }

  updateSystemStatus(data) {
    // WiFi status
    const wifiConnected = data.wifi_connected || false;
    const wifiStatusEl = document.getElementById("wifiStatus");
    if (wifiConnected) {
      wifiStatusEl.textContent = data.wifi_ssid || "Connected";
      wifiStatusEl.style.color = "var(--success-color)";
    } else {
      wifiStatusEl.textContent = "AP Mode";
      wifiStatusEl.style.color = "var(--warning-color)";
    }

    // IP address
    document.getElementById("ipAddress").textContent = data.wifi_ip || "-";

    // Uptime
    if (data.uptime_ms) {
      document.getElementById("uptime").textContent = this.formatUptime(
        data.uptime_ms,
      );
    }

    // CAN message count in status bar
    if (data.can_message_count !== undefined) {
      document.getElementById("canMessageCount").textContent =
        this.formatNumber(data.can_message_count);
    }

    // System info in settings
    if (data.chip_model) {
      document.getElementById("chipModel").textContent = data.chip_model;
    }
    if (data.cpu_freq_mhz) {
      document.getElementById("cpuFreq").textContent =
        data.cpu_freq_mhz + " MHz";
    }
    if (data.free_heap) {
      document.getElementById("freeHeap").textContent =
        (data.free_heap / 1024).toFixed(1) + " KB";
    }
    if (data.sdk_version) {
      document.getElementById("sdkVersion").textContent = data.sdk_version;
    }
    if (data.can_message_count !== undefined) {
      document.getElementById("canMsgTotal").textContent = this.formatNumber(
        data.can_message_count,
      );
    }
    if (data.can_dropped_count !== undefined) {
      document.getElementById("canMsgDropped").textContent = this.formatNumber(
        data.can_dropped_count,
      );
    }
  }

  // API Calls
  async loadConfig() {
    try {
      const response = await fetch("/api/config");
      if (response.ok) {
        this.config = await response.json();
        this.populateConfigForm();
      }
    } catch (error) {
      console.error("Error loading config:", error);
    }
  }

  populateConfigForm() {
    // WiFi
    document.getElementById("wifiSSID").value = this.config.wifi_ssid || "";

    // MQTT
    document.getElementById("mqttEnabled").checked = this.config.mqtt_enabled !== false;
    document.getElementById("mqttBroker").value = this.config.mqtt_broker || "";
    document.getElementById("mqttPort").value = this.config.mqtt_port || 1883;
    document.getElementById("mqttUsername").value =
      this.config.mqtt_username || "";
    document.getElementById("mqttTopicPrefix").value =
      this.config.mqtt_topic_prefix || "ebike";

    // CAN Logging
    document.getElementById("canLogEnabled").checked =
      this.config.can_log_enabled !== false;
    document.getElementById("mqttCanmsgEnabled").checked =
      this.config.mqtt_canmsg_enabled !== false;
  }

  async saveWiFiConfig() {
    const ssid = document.getElementById("wifiSSID").value.trim();
    const password = document.getElementById("wifiPassword").value;

    if (!ssid) {
      this.showToast("SSID is required", "error");
      return;
    }

    const config = {
      wifi_ssid: ssid,
    };

    // Only include password if it's not empty
    if (password) {
      config.wifi_password = password;
    }

    try {
      const response = await fetch("/api/config", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify(config),
      });

      const result = await response.json();

      if (response.ok && result.success) {
        this.showToast("WiFi settings saved! Rebooting to apply...", "success");

        // Clear password field
        document.getElementById("wifiPassword").value = "";

        // Reboot after 2 seconds
        setTimeout(() => {
          this.rebootDevice();
        }, 2000);
      } else {
        this.showToast(
          result.message || "Failed to save WiFi settings",
          "error",
        );
      }
    } catch (error) {
      console.error("Error saving WiFi config:", error);
      this.showToast("Network error - check connection", "error");
    }
  }

  async saveMQTTConfig() {
    const config = {
      mqtt_enabled: document.getElementById("mqttEnabled").checked,
      mqtt_broker: document.getElementById("mqttBroker").value.trim(),
      mqtt_port: parseInt(document.getElementById("mqttPort").value) || 1883,
      mqtt_username: document.getElementById("mqttUsername").value.trim(),
      mqtt_password: document.getElementById("mqttPassword").value,
      mqtt_topic_prefix:
        document.getElementById("mqttTopicPrefix").value.trim() || "ebike",
    };

    try {
      const response = await fetch("/api/config", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify(config),
      });

      const result = await response.json();

      if (response.ok && result.success) {
        this.showToast("MQTT settings saved!", "success");
        document.getElementById("mqttPassword").value = "";
      } else {
        this.showToast(
          result.message || "Failed to save MQTT settings",
          "error",
        );
      }
    } catch (error) {
      console.error("Error saving MQTT config:", error);
      this.showToast("Network error - check connection", "error");
    }
  }

  async saveCANLoggingConfig() {
    const canLogEl = document.getElementById("canLogEnabled");
    const mqttCanmsgEl = document.getElementById("mqttCanmsgEnabled");

    if (!canLogEl || !mqttCanmsgEl) {
      this.showToast("Error: form elements not found", "error");
      return;
    }

    const config = {
      can_log_enabled: canLogEl.checked,
      mqtt_canmsg_enabled: mqttCanmsgEl.checked,
    };

    try {
      const response = await fetch("/api/config", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify(config),
      });

      const result = await response.json();

      if (response.ok && result.success) {
        this.showToast("CAN logging settings saved!", "success");
        // Reload config to update the form
        this.loadConfig();
      } else {
        this.showToast(
          result.message || "Failed to save CAN settings",
          "error",
        );
      }
    } catch (error) {
      console.error("Error saving CAN logging config:", error);
      this.showToast("Network error - check connection", "error");
    }
  }

  async rebootDevice() {
    try {
      await fetch("/api/reset", {
        method: "POST",
      });

      this.showToast("Device rebooting...", "warning");
      this.hideModal();

      // Close WebSocket
      if (this.ws) {
        this.ws.close();
      }

      // Show reconnecting message
      setTimeout(() => {
        this.showToast("Waiting for device to restart...", "warning");
      }, 3000);
    } catch (error) {
      console.error("Error rebooting device:", error);
    }
  }

  async clearWiFi() {
    // This would need to be implemented as a specific API endpoint
    // For now, we'll just show a message
    this.showToast("Please use serial command: reset_wifi", "warning");
  }

  // Periodic Updates (fallback if WebSocket fails)
  startPeriodicUpdates() {
    setInterval(() => {
      // If WebSocket is not connected, fetch data via HTTP
      if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
        this.fetchStatus();
      }
    }, 5000);
  }

  async fetchStatus() {
    try {
      const response = await fetch("/api/status");
      if (response.ok) {
        const data = await response.json();
        if (data.system) {
          this.updateSystemStatus(data.system);
        }
        if (data.batteries) {
          this.updateBatteries(data.batteries);
        }
      }
    } catch (error) {
      console.error("Error fetching status:", error);
    }
  }

  // UI Helpers
  showModal() {
    document.getElementById("settingsModal").classList.add("active");
    // Reload config when opening settings
    this.loadConfig();
  }

  hideModal() {
    document.getElementById("settingsModal").classList.remove("active");
  }

  showToast(message, type = "info") {
    const toast = document.getElementById("toast");
    toast.textContent = message;
    toast.className = "toast show " + type;

    setTimeout(() => {
      toast.classList.remove("show");
    }, 3000);
  }

  formatUptime(ms) {
    const seconds = Math.floor(ms / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);

    if (days > 0) {
      return `${days}d ${hours % 24}h`;
    } else if (hours > 0) {
      return `${hours}h ${minutes % 60}m`;
    } else if (minutes > 0) {
      return `${minutes}m ${seconds % 60}s`;
    } else {
      return `${seconds}s`;
    }
  }

  formatNumber(num) {
    return num.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
  }

  escapeHtml(text) {
    const map = {
      "&": "&amp;",
      "<": "&lt;",
      ">": "&gt;",
      '"': "&quot;",
      "'": "&#039;",
    };
    return text.replace(/[&<>"']/g, (m) => map[m]);
  }

  // CAN Monitor Methods
  handleCANMessage(message) {
    // console.log("CAN message received:", message);
    if (this.canMonitor.paused) return;

    // Apply filter if set
    if (this.canMonitor.filter) {
      const msgId = message.id.toLowerCase();
      const filter = this.canMonitor.filter.toLowerCase();
      if (!msgId.includes(filter)) return;
    }

    const viewer = document.getElementById("canLogViewer");
    if (!viewer) return;

    // Format timestamp
    const now = new Date();
    const timestamp = now.toLocaleTimeString() + "." + now.getMilliseconds().toString().padStart(3, "0");

    // Format message line
    const line = `[${timestamp}] ID:${message.id} DLC:${message.dlc} Data:${message.data}\n`;

    // Append to viewer
    viewer.value += line;

    // Update counter
    this.canMonitor.messageCount++;
    document.getElementById("canMessageCount").textContent = this.formatNumber(this.canMonitor.messageCount);

    // Limit total lines to prevent memory issues
    const lines = viewer.value.split("\n");
    if (lines.length > this.canMonitor.maxMessages) {
      viewer.value = lines.slice(lines.length - this.canMonitor.maxMessages).join("\n");
    }

    // Auto-scroll to bottom
    viewer.scrollTop = viewer.scrollHeight;
  }

  toggleCANMonitorPause() {
    this.canMonitor.paused = !this.canMonitor.paused;
    const btn = document.getElementById("canPauseBtn");
    const indicator = document.getElementById("canPausedIndicator");

    if (this.canMonitor.paused) {
      btn.textContent = "Resume";
      btn.style.background = "#f59e0b";
      indicator.style.display = "inline";
    } else {
      btn.textContent = "Pause";
      btn.style.background = "";
      indicator.style.display = "none";
    }
  }

  clearCANMonitor() {
    document.getElementById("canLogViewer").value = "";
    this.canMonitor.messageCount = 0;
    document.getElementById("canMessageCount").textContent = "0";
    this.showToast("CAN monitor cleared", "success");
  }

  copyCANMonitor() {
    const viewer = document.getElementById("canLogViewer");
    if (!viewer.value) {
      this.showToast("Nothing to copy", "warning");
      return;
    }

    navigator.clipboard.writeText(viewer.value)
      .then(() => {
        this.showToast("Copied to clipboard!", "success");
      })
      .catch(() => {
        // Fallback for older browsers
        viewer.select();
        document.execCommand("copy");
        this.showToast("Copied to clipboard!", "success");
      });
  }

  setCANFilter(value) {
    this.canMonitor.filter = value || null;
    const filterText = value ? ` (filtered by ${value})` : "";
    console.log(`CAN filter ${value ? "set to: " + value : "cleared"}`);
  }
}

// Initialize app when DOM is ready
document.addEventListener("DOMContentLoaded", () => {
  window.app = new BatteryMonitor();
});
