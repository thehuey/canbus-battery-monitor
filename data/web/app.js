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
      excludeList: [], // Array of IDs to exclude (lowercase for comparison)
      visible: false,  // Only true when CAN Monitor page is active
    };

    // CAN Analyzer (IndexedDB + statistics)
    this.canAnalyzer = null;
    this.statsDecodeMode = 'hex';

    // Protocol Manager (browser-side CAN message parsing)
    this.protocolManager = null;

    // Developer Tools (message sending)
    this.devTools = null;

    // Protocol UI update batching
    this._protocolUIUpdatePending = false;

    this.init();
  }

  async init() {
    // Initialize Protocol Manager (browser-side CAN parsing)
    if (window.ProtocolManager) {
      this.protocolManager = new window.ProtocolManager();
      await this.protocolManager.ready;
      console.debug("[App] Protocol Manager initialized");
      this.updateProtocolList();

      // Wire up battery state updates from protocol-parsed CAN messages
      this.protocolManager.onBatteryUpdate = (state) => {
        this.updateBatteryFromProtocol(state);
      };
    }

    // Initialize CAN Analyzer (wait for IndexedDB)
    if (window.CANAnalyzer) {
      this.canAnalyzer = new window.CANAnalyzer();
      console.debug("[App] CAN Analyzer initialized");
    }

    // Initialize Developer Tools
    if (window.DevTools) {
      this.devTools = new window.DevTools();
      console.debug("[App] Developer Tools initialized");
    }

    this.setupEventListeners();
    this.initRouter();
    this.setupASCIIDetection();
    this.loadConfig();
    this.connectWebSocket();
    this.startPeriodicUpdates();

    // Auto-select first protocol if ProtocolManager loaded one
    if (this.protocolManager && this.protocolManager.activeProtocol) {
      console.debug("[App] Protocol auto-selected:", this.protocolManager.activeProtocol.name);
    }
  }

  // ==========================================
  // SPA Router
  // ==========================================

  initRouter() {
    // Tab button clicks
    document.querySelectorAll('.tab-btn[data-tab]').forEach(btn => {
      btn.addEventListener('click', () => {
        this.navigateTo(btn.dataset.tab);
      });
    });

    // Settings tab clicks (event delegation)
    const settingsTabBar = document.querySelector('.settings-tab-bar');
    if (settingsTabBar) {
      settingsTabBar.addEventListener('click', (e) => {
        const btn = e.target.closest('.settings-tab-btn');
        if (!btn) return;
        this.showSettingsTab(btn.dataset.settingsTab);
      });
    }

    // Handle browser back/forward
    window.addEventListener('hashchange', () => {
      this.showPage(this.getPageFromHash());
    });

    // Show initial page from hash (or default to dashboard)
    this.showPage(this.getPageFromHash());
  }

  getPageFromHash() {
    const hash = location.hash.replace('#', '');
    return ['dashboard', 'can', 'devtools'].includes(hash) ? hash : 'dashboard';
  }

  navigateTo(page) {
    location.hash = '#' + page;
  }

  showPage(page) {
    // Update page containers
    document.querySelectorAll('.page').forEach(el => {
      el.style.display = 'none';
    });
    const target = document.getElementById('page-' + page);
    if (target) target.style.display = '';

    // Update tab buttons
    document.querySelectorAll('.tab-btn[data-tab]').forEach(btn => {
      btn.classList.toggle('active', btn.dataset.tab === page);
    });

    // CAN monitor visibility flag
    this.canMonitor.visible = (page === 'can');
  }

  showSettingsTab(tab) {
    document.querySelectorAll('.settings-page').forEach(el => {
      el.classList.remove('active');
    });
    document.querySelectorAll('.settings-tab-btn').forEach(btn => {
      btn.classList.toggle('active', btn.dataset.settingsTab === tab);
    });
    const target = document.getElementById('settings-' + tab);
    if (target) target.classList.add('active');
  }

  setupEventListeners() {
    // Close WebSocket when page is hidden or closed
    document.addEventListener("visibilitychange", () => {
      if (document.hidden) {
        console.debug("Page hidden, closing WebSocket");
        if (this.ws) {
          this.ws.close();
        }
        this.clearReconnectInterval();
      } else {
        console.debug("Page visible, reconnecting WebSocket");
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

    // Protocol selector — use ProtocolManager for protocol switching
    const protocolSelect = document.getElementById("protocolSelect");
    if (protocolSelect) {
      protocolSelect.addEventListener("change", (e) => {
        const protocolId = e.target.value;
        if (protocolId && this.protocolManager) {
          this.setActiveProtocol(protocolId);
        }
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

    // CAN Exclude filter
    document.getElementById("canExcludeInput").addEventListener("input", (e) => {
      this.setCANExcludeFilter(e.target.value.trim());
    });

    // CAN Stats button
    const statsBtn = document.getElementById("canStatsBtn");
    if (statsBtn) {
      statsBtn.addEventListener("click", () => {
        this.showCANStats();
      });
    }

    // Stats modal close
    const closeStats = document.getElementById("closeStats");
    if (closeStats) {
      closeStats.addEventListener("click", () => {
        document.getElementById("statsModal").classList.remove("active");
      });
    }

    const statsModal = document.getElementById("statsModal");
    if (statsModal) {
      statsModal.addEventListener("click", (e) => {
        if (e.target.id === "statsModal") {
          statsModal.classList.remove("active");
        }
      });
    }

    // Stats clear button
    const statsClearBtn = document.getElementById("statsClearBtn");
    if (statsClearBtn) {
      statsClearBtn.addEventListener("click", async () => {
        if (confirm("Clear all analyzer data? This cannot be undone.")) {
          if (this.canAnalyzer) {
            await this.canAnalyzer.clearAllData();
            this.showCANStats(); // Refresh the view
            this.showToast("All analyzer data cleared", "success");
          }
        }
      });
    }

    // Decode toggle buttons
    const decodeToggle = document.getElementById("decodeToggle");
    if (decodeToggle) {
      decodeToggle.addEventListener("click", (e) => {
        const btn = e.target.closest(".decode-btn");
        if (!btn) return;
        decodeToggle.querySelectorAll(".decode-btn").forEach(b => b.classList.remove("active"));
        btn.classList.add("active");
        this.statsDecodeMode = btn.dataset.mode;
        this.refreshStatsData();
      });
    }

    // Developer Tools - Export/Import
    const exportBtn = document.getElementById("devExportBtn");
    if (exportBtn) {
      exportBtn.addEventListener("click", () => {
        this.exportAnalyzerData();
      });
    }

    const importBtn = document.getElementById("devImportBtn");
    const importFile = document.getElementById("devImportFile");
    if (importBtn && importFile) {
      importBtn.addEventListener("click", () => {
        importFile.click();
      });

      importFile.addEventListener("change", (e) => {
        const file = e.target.files[0];
        if (file) {
          this.importAnalyzerData(file);
        }
        e.target.value = ""; // Reset file input
      });
    }

    // Developer Tools - Send Message
    const sendBtn = document.getElementById("devSendBtn");
    if (sendBtn) {
      sendBtn.addEventListener("click", () => {
        this.sendCANMessage();
      });
    }

    // Developer Tools - History
    const histBtn = document.getElementById("devSendHistBtn");
    if (histBtn) {
      histBtn.addEventListener("click", () => {
        this.toggleSendHistory();
      });
    }

    const histClearBtn = document.getElementById("devHistoryClearBtn");
    if (histClearBtn) {
      histClearBtn.addEventListener("click", () => {
        this.clearSendHistory();
      });
    }

    // Developer Tools - Batch Send
    const batchSendBtn = document.getElementById("devBatchSendBtn");
    if (batchSendBtn) {
      batchSendBtn.addEventListener("click", () => {
        this.sendBatchMessages();
      });
    }

    const batchClearBtn = document.getElementById("devBatchClearBtn");
    if (batchClearBtn) {
      batchClearBtn.addEventListener("click", () => {
        document.getElementById("devBatchMessages").value = "";
      });
    }

    // Protocol selection button (settings modal uses separate select ID)
    const saveProtocolBtn = document.getElementById("saveProtocolBtn");
    if (saveProtocolBtn) {
      saveProtocolBtn.addEventListener("click", () => {
        const select = document.getElementById("settingsProtocolSelect");
        if (select && select.value) {
          this.setActiveProtocol(select.value);
          // Sync main page selector
          const mainSelect = document.getElementById("protocolSelect");
          if (mainSelect) mainSelect.value = select.value;
        }
      });
    }
  }

  // Update protocol list in both main page and settings selectors
  updateProtocolList() {
    if (!this.protocolManager) return;

    const protocols = this.protocolManager.getAvailableProtocols();
    const activeId = this.protocolManager.activeProtocolId
      || (protocols.length > 0 ? protocols[0].id : null);

    // Populate both selectors
    for (const selectId of ["protocolSelect", "settingsProtocolSelect"]) {
      const select = document.getElementById(selectId);
      if (!select) continue;

      select.innerHTML = "";
      for (const proto of protocols) {
        const option = document.createElement("option");
        option.value = proto.id;
        const name = proto.name || proto.id;
        const mfg = proto.manufacturer;
        option.textContent = mfg ? `${name} (${mfg})` : name;
        select.appendChild(option);
      }

      if (activeId) {
        select.value = activeId;
      }
    }
  }

  // Set active protocol (async — fetches full protocol JSON on demand)
  async setActiveProtocol(protocolId) {
    if (!this.protocolManager) {
      this.showToast("Protocol manager not initialized", "error");
      return;
    }

    const success = await this.protocolManager.setActiveProtocol(protocolId);
    if (success) {
      this.showToast(`Protocol set to: ${this.protocolManager.activeProtocol.name}`, "success");
    } else {
      this.showToast("Failed to set protocol", "error");
    }
  }

  // WebSocket Management
  connectWebSocket() {
    // Prevent multiple simultaneous connection attempts
    if (
      this.ws &&
      (this.ws.readyState === WebSocket.CONNECTING ||
        this.ws.readyState === WebSocket.OPEN)
    ) {
      console.debug("WebSocket already connecting or connected, skipping");
      return;
    }

    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const wsUrl = `${protocol}//${window.location.host}/ws`;

    console.debug("Connecting to WebSocket:", wsUrl);

    try {
      // Close existing connection if any
      if (this.ws) {
        this.ws.close();
        this.ws = null;
      }

      this.ws = new WebSocket(wsUrl);

      // Set binary type to arraybuffer (not blob) for efficient binary handling
      this.ws.binaryType = "arraybuffer";

      this.ws.onopen = () => {
        console.debug("WebSocket connected");
        this.showToast("Connected", "success");
        this.clearReconnectInterval();

        // Pass WebSocket to DevTools for sending messages
        if (this.devTools) {
          this.devTools.setWebSocket(this.ws);
        }
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
        console.debug("WebSocket disconnected");
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
        console.debug("WebSocket message:", message.type, message);
      }

      switch (message.type) {
        case "battery_update":
          // Ignored — battery data now comes from protocol-parsed CAN messages
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
      console.debug("Reconnect already scheduled");
      return;
    }

    console.debug("Scheduling reconnect in 1 second...");
    this.reconnectInterval = setInterval(() => {
      console.debug("Attempting to reconnect...");
      this.connectWebSocket();
    }, 1000);
  }

  clearReconnectInterval() {
    if (this.reconnectInterval) {
      console.debug("Clearing reconnect interval");
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

  // Update battery display from protocol-parsed CAN messages (batched via rAF)
  updateBatteryFromProtocol(_state) {
    if (this._protocolUIUpdatePending) return;
    this._protocolUIUpdatePending = true;

    requestAnimationFrame(() => {
      this._protocolUIUpdatePending = false;
      if (!this.protocolManager) return;

      const state = this.protocolManager.batteryState;
      const voltage = state.voltage || 0;
      const current = state.current || 0;
      const power = state.power || (voltage * current);

      // Update summary metrics
      document.getElementById("totalPower").textContent = power.toFixed(1) + " W";
      document.getElementById("totalCurrent").textContent = current.toFixed(2) + " A";
      document.getElementById("avgVoltage").textContent = voltage.toFixed(1) + " V";

      // Build a battery object compatible with createBatteryCard
      const battery = {
        id: 1,
        name: this.protocolManager.activeProtocol
          ? this.protocolManager.activeProtocol.name
          : "Battery 1",
        voltage: voltage,
        current: current,
        power: power,
        soc: state.remaining_mah || 0,
        max_soc: state.max_mah || 0,
        pack_identifier: state.pack_identifier || null,
        bms_info: state.bms_info || null,
        state: state.state !== undefined ? state.state : null,
        state_name: state.state_name || '',
        has_error: false,
      };

      // Add cell voltages (array of mV values)
      if (state.cell_voltages_mv && state.cell_voltages_mv.some(v => v > 0)) {
        battery.cell_voltages = state.cell_voltages_mv;
      }

      // Update the battery cards container
      const container = document.getElementById("batteriesContainer");
      if (!container) return;

      container.innerHTML = "";
      container.appendChild(this.createBatteryCard(battery));
    });
  }

  createBatteryCard(battery) {
    const card = document.createElement("div");
    card.className = "battery-card";
    if (battery.has_error) {
      card.classList.add("error");
    }

    const statusClass = battery.has_error ? "error" : "ok";
    const statusText = battery.has_error ? "Error"
      : (battery.state_name ? battery.state_name : "OK");

    // Parse pack identifier if available
    let packInfoHTML = "";
    if (battery.pack_identifier || battery.bms_info) {
      let infoItems = "";
      if (battery.pack_identifier) {
        const packInfo = this.parsePackIdentifier(battery.pack_identifier);
        if (packInfo) {
          infoItems += `
                        <div class="info-item">
                            <span class="info-label">Mfg:</span>
                            <span class="info-text">${packInfo.date}</span>
                        </div>
                        <div class="info-item">
                            <span class="info-label">S/N:</span>
                            <span class="info-text">${packInfo.serial}</span>
                        </div>`;
        }
      }
      if (battery.bms_info) {
        infoItems += `
                        <div class="info-item">
                            <span class="info-label">BMS:</span>
                            <span class="info-text">${this.escapeHtml(battery.bms_info)}</span>
                        </div>`;
      }
      if (infoItems) {
        packInfoHTML = `<div class="battery-info">${infoItems}</div>`;
      }
    }

    // SOC display with max_soc and percentage
    const socValue = battery.soc || 0;
    const maxSoc = battery.max_soc || 0;
    let socDisplay;
    let socPct = 0;
    if (maxSoc > 0) {
      socPct = (socValue / maxSoc) * 100;
      socDisplay = `${socPct.toFixed(1)}% <span class="metric-unit">(${socValue}/${maxSoc} mAh)</span>`;
    } else {
      socDisplay = `${socValue}`;
    }

    // Runtime estimation
    const runtime = this.estimateRuntime(battery);
    const runtimeDisplay = this.formatRuntime(runtime);
    const runtimeClass = runtime && runtime.totalMinutes < 30 ? 'runtime-low' :
                         runtime && runtime.totalMinutes < 60 ? 'runtime-warn' : '';

    // Amp gauge (max 30A for ACS712-30A)
    const maxAmps = 30;
    const currentAbs = Math.abs(battery.current || 0);
    const gaugePct = this.ampGaugePercent(battery.current, maxAmps);
    const gaugeAngle = 180 * gaugePct;
    const gaugeColor = gaugePct > 0.8 ? 'var(--danger-color)' :
                       gaugePct > 0.5 ? 'var(--warning-color)' : 'var(--success-color)';

    // Cell voltages display
    let cellVoltagesHTML = "";
    if (battery.cell_voltages && battery.cell_voltages.length > 0) {
      const cells = battery.cell_voltages.map((mv, i) =>
        `<div class="cell-voltage"><span class="cell-label">C${i + 1}</span><span class="cell-value">${mv}</span></div>`
      ).join("");
      cellVoltagesHTML = `
            <div class="cell-voltages-section">
                <div class="cell-voltages-header">Cell Voltages (mV)</div>
                <div class="cell-voltages-grid">${cells}</div>
            </div>`;
    }

    // SOC progress bar
    let socBarHTML = "";
    if (maxSoc > 0) {
      const pctNum = Math.min((socValue / maxSoc) * 100, 100);
      const socClass = pctNum > 20 ? 'soc-good' : pctNum > 10 ? 'soc-warn' : 'soc-crit';
      socBarHTML = `
            <div class="soc-bar">
                <div class="soc-bar-fill ${socClass}" style="width: ${pctNum.toFixed(1)}%"></div>
            </div>`;
    }

    card.innerHTML = `
            <div class="battery-header">
                <div class="battery-name">${this.escapeHtml(battery.name || `Battery ${battery.id}`)}</div>
                <div class="battery-status ${statusClass}">${statusText}</div>
            </div>
            ${packInfoHTML}
            <div class="amp-gauge-row">
                <div class="amp-gauge">
                    <svg viewBox="0 0 120 70" class="amp-gauge-svg">
                        <path d="M 10 65 A 50 50 0 0 1 110 65" fill="none" stroke="var(--bg-card)" stroke-width="8" stroke-linecap="round"/>
                        <path d="M 10 65 A 50 50 0 0 1 110 65" fill="none" stroke="${gaugeColor}" stroke-width="8" stroke-linecap="round"
                              stroke-dasharray="${Math.PI * 50}" stroke-dashoffset="${Math.PI * 50 * (1 - gaugePct)}"/>
                        <line x1="60" y1="65" x2="${60 + 45 * Math.cos(Math.PI - gaugeAngle * Math.PI / 180)}" y2="${65 - 45 * Math.sin(gaugeAngle * Math.PI / 180)}"
                              stroke="var(--text-primary)" stroke-width="2" stroke-linecap="round"/>
                        <circle cx="60" cy="65" r="3" fill="var(--text-primary)"/>
                    </svg>
                    <div class="amp-gauge-value">${currentAbs.toFixed(1)}<span class="amp-gauge-unit">A</span></div>
                    <div class="amp-gauge-label">Current Draw</div>
                </div>
                <div class="runtime-display ${runtimeClass}">
                    <div class="runtime-value">${runtimeDisplay}</div>
                    <div class="runtime-label">Est. Runtime</div>
                    ${socPct > 0 ? `<div class="soc-bar-container"><div class="soc-bar" style="width:${Math.min(socPct, 100)}%"></div></div>` : ''}
                </div>
            </div>
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
                    <span class="metric-value">${socDisplay}</span>
                </div>
            </div>
            ${socBarHTML}
            ${cellVoltagesHTML}
        `;

    return card;
  }


  // ==========================================
  // Runtime estimation from SOC + current draw
  // ==========================================

  estimateRuntime(battery) {
    const current = Math.abs(battery.current || 0);
    const socValue = battery.soc || 0;
    const maxSoc = battery.max_soc || 0;

    if (current < 0.05 || maxSoc <= 0 || socValue <= 0) {
      return null; // Not enough data
    }

    // SOC values are in mAh; remaining capacity = socValue mAh
    // Runtime (hours) = remaining mAh / (current * 1000) since current is in A
    const remainingAh = socValue / 1000;
    const hours = remainingAh / current;
    const totalMinutes = Math.round(hours * 60);

    if (totalMinutes < 1 || totalMinutes > 9999) return null;

    const h = Math.floor(totalMinutes / 60);
    const m = totalMinutes % 60;
    return { hours: h, minutes: m, totalMinutes };
  }

  formatRuntime(rt) {
    if (!rt) return '--:--';
    return `${rt.hours}h ${String(rt.minutes).padStart(2, '0')}m`;
  }

  // Compute amp gauge angle (0-180 degrees) for semicircle gauge
  ampGaugePercent(current, maxAmps) {
    const abs = Math.abs(current || 0);
    return Math.min(abs / maxAmps, 1.0);
  }


  /**
   * Parse D-power pack identifier format: YYDDMMSSSS
   * @param {number} identifier - The 32-bit pack identifier value
   * @returns {object|null} - Parsed date and serial, or null if invalid
   */
  parsePackIdentifier(identifier) {
    if (!identifier || identifier <= 0) return null;

    try {
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
    document.getElementById("mqttEnabled").checked =
      this.config.mqtt_enabled !== false;
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
    // Parse CAN message with protocol manager for battery data extraction
    if (this.protocolManager) {
      this.protocolManager.processCANMessage(message);
    }

    // Analyze message with CAN Analyzer
    if (this.canAnalyzer) {
      this.canAnalyzer.analyzeMessage(message);
    }

    // Skip DOM updates when CAN Monitor page is not visible or paused
    if (this.canMonitor.paused || !this.canMonitor.visible) return;

    // Apply include filter if set
    if (this.canMonitor.filter) {
      const msgId = message.id.toLowerCase();
      const filter = this.canMonitor.filter.toLowerCase();
      if (!msgId.includes(filter)) return;
    }

    // Apply exclude filter if set
    if (this.canMonitor.excludeList.length > 0) {
      const msgId = message.id.toLowerCase();
      if (this.canMonitor.excludeList.includes(msgId)) return;
    }

    const viewer = document.getElementById("canLogViewer");
    if (!viewer) return;

    // Format timestamp
    const now = new Date();
    const timestamp =
      now.toLocaleTimeString() +
      "." +
      now.getMilliseconds().toString().padStart(3, "0");

    // Format message line
    const line = `[${timestamp}] ID:${message.id} DLC:${message.dlc} Data:${message.data}\n`;

    // Append to viewer
    viewer.value += line;

    // Note: Do NOT increment local counter - use server's official count instead
    // The server sends the authoritative message count from the logger every 5 seconds
    // Incrementing locally causes "jumping chunks" when server updates override local count

    // Limit total lines to prevent memory issues
    const lines = viewer.value.split("\n");
    if (lines.length > this.canMonitor.maxMessages) {
      viewer.value = lines
        .slice(lines.length - this.canMonitor.maxMessages)
        .join("\n");
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
    const el = document.getElementById("canMonitorCount");
    if (el) el.textContent = "0 messages";
    this.showToast("CAN monitor cleared", "success");
  }

  copyCANMonitor() {
    const viewer = document.getElementById("canLogViewer");
    if (!viewer.value) {
      this.showToast("Nothing to copy", "warning");
      return;
    }

    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard
        .writeText(viewer.value)
        .then(() => {
          this.showToast("Copied to clipboard!", "success");
        })
        .catch(() => {
          this.fallbackCopy(viewer);
        });
    } else {
      this.fallbackCopy(viewer);
    }
  }

  fallbackCopy(textarea) {
    textarea.select();
    document.execCommand("copy");
    this.showToast("Copied to clipboard!", "success");
  }

  setCANFilter(value) {
    this.canMonitor.filter = value || null;
    console.debug(`CAN filter ${value ? "set to: " + value : "cleared"}`);
  }

  setCANExcludeFilter(value) {
    // Parse comma-separated list of IDs to exclude
    if (value) {
      // Split by comma, normalize to lowercase, remove spaces
      this.canMonitor.excludeList = value
        .split(',')
        .map(id => id.trim().toLowerCase())
        .filter(id => id.length > 0);
      console.debug(`CAN exclude filter set to: ${this.canMonitor.excludeList.join(', ')}`);
    } else {
      this.canMonitor.excludeList = [];
      console.debug("CAN exclude filter cleared");
    }
  }

  // ASCII detection is now handled inline by the analyzer.
  // No separate event system needed - results are shown in the stats UI.
  setupASCIIDetection() {
    // Placeholder - ASCII detection is integrated into can_analyzer.js
  }

  // Export analyzer data
  async exportAnalyzerData() {
    if (!this.canAnalyzer) {
      this.showToast("Analyzer not available", "error");
      return;
    }

    try {
      const data = await this.canAnalyzer.exportData();
      const json = JSON.stringify(data, null, 2);

      // Use data: URI instead of blob: to work over plain HTTP
      const a = document.createElement("a");
      a.href = "data:application/json;charset=utf-8," + encodeURIComponent(json);
      a.download = `can_analysis_${Date.now()}.json`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);

      this.showToast("Data exported successfully", "success");
    } catch (error) {
      console.error("Export failed:", error);
      this.showToast("Export failed", "error");
    }
  }

  // Import analyzer data
  async importAnalyzerData(file) {
    if (!this.canAnalyzer) {
      this.showToast("Analyzer not available", "error");
      return;
    }

    try {
      const text = await file.text();
      const data = JSON.parse(text);
      const success = await this.canAnalyzer.importData(data);

      if (success) {
        this.showToast("Data imported successfully", "success");
      } else {
        this.showToast("Import failed", "error");
      }
    } catch (error) {
      console.error("Import failed:", error);
      this.showToast("Import failed - invalid file", "error");
    }
  }

  // Show CAN statistics in a modal UI
  showCANStats() {
    try {
      if (!this.canAnalyzer) {
        this.showToast("Analyzer not loaded - check browser console", "error");
        console.error("[Stats] canAnalyzer is null. window.CANAnalyzer:", typeof window.CANAnalyzer);
        return;
      }

      const modal = document.getElementById("statsModal");
      const container = document.getElementById("statsTableContainer");
      const countEl = document.getElementById("statsCount");

      if (!modal || !container || !countEl) {
        this.showToast("Stats UI elements missing - re-upload index.html", "error");
        console.error("[Stats] Missing DOM elements:", { modal: !!modal, container: !!container, countEl: !!countEl });
        return;
      }

      const stats = this.canAnalyzer.getAllStats();
      const totalMsgs = stats.reduce((s, e) => s + e.count, 0);

      countEl.textContent = `${stats.length} unique IDs, ${totalMsgs.toLocaleString()} total msgs`;

      if (stats.length === 0) {
        container.innerHTML = '<div class="loading">No CAN messages captured yet</div>';
      } else {
        container.innerHTML = this.buildStatsTable(stats);
        this.attachStatsHandlers(container);
      }

      modal.classList.add("active");
    } catch (error) {
      console.error("[Stats] showCANStats failed:", error);
      this.showToast("Stats error: " + error.message, "error");
    }
  }

  refreshStatsData() {
    if (!this.canAnalyzer) return;
    const container = document.getElementById("statsTableContainer");
    if (!container) return;

    const stats = this.canAnalyzer.getAllStats();
    if (stats.length === 0) return;

    container.innerHTML = this.buildStatsTable(stats);
    this.attachStatsHandlers(container);
  }

  buildStatsTable(stats) {
    const modeLabels = { hex: 'HEX', ascii: 'ASCII', 'dec-byte': 'DEC', 'dec-16le': '16-LE' };
    const dataHeader = 'Last Data (' + (modeLabels[this.statsDecodeMode] || 'HEX') + ')';
    let html = `<table class="stats-table">
      <thead><tr>
        <th>ID</th>
        <th>Count</th>
        <th>DLC</th>
        <th>Interval</th>
        <th>Unique</th>
        <th>${dataHeader}</th>
        <th>ASCII</th>
      </tr></thead><tbody>`;

    for (const s of stats) {
      const uniqueData = this.canAnalyzer.getUniqueData(s.id);
      const lastEntry = uniqueData.length > 0 ? uniqueData[0] : null;
      const lastHex = lastEntry ? this.formatDataForMode(lastEntry.hex) : '-';
      const interval = s.avgInterval > 0 ? Math.round(s.avgInterval) + 'ms' : '-';

      // Find any ASCII values among unique data
      const asciiEntries = uniqueData.filter(d => d.ascii);
      let asciiCol = '';
      if (asciiEntries.length > 0) {
        asciiCol = `<span class="ascii-badge">ASCII</span> ${this.escapeHtml(asciiEntries[0].ascii)}`;
      }

      html += `<tr class="stats-row" data-id="${this.escapeHtml(s.id)}">
        <td class="id-cell">${this.escapeHtml(s.id)}</td>
        <td class="count-cell">${s.count.toLocaleString()}</td>
        <td>${s.dlc}</td>
        <td class="interval-cell">${interval}</td>
        <td>${uniqueData.length}</td>
        <td>${lastHex}</td>
        <td>${asciiCol}</td>
      </tr>`;

      // Expandable detail row
      html += `<tr class="stats-detail" data-detail-id="${this.escapeHtml(s.id)}">
        <td colspan="7">
          <div class="data-list">`;

      for (const d of uniqueData) {
        const hexFormatted = this.formatDataForMode(d.hex);
        const asciiPart = d.ascii ? `<span class="data-ascii">"${this.escapeHtml(d.ascii)}"</span>` : '';
        html += `<div class="data-entry">
          <span class="data-hex">${hexFormatted}</span>
          ${asciiPart}
          <span class="data-count">${d.count.toLocaleString()}x</span>
        </div>`;
      }

      html += `</div></td></tr>`;
    }

    html += '</tbody></table>';
    return html;
  }

  attachStatsHandlers(container) {
    container.querySelectorAll('.stats-row').forEach(row => {
      row.addEventListener('click', () => {
        const id = row.dataset.id;
        const detailRow = container.querySelector(`[data-detail-id="${id}"]`);
        if (!detailRow) return;

        const isExpanded = detailRow.classList.contains('expanded');
        // Collapse all
        container.querySelectorAll('.stats-detail').forEach(d => d.classList.remove('expanded'));
        container.querySelectorAll('.stats-row').forEach(r => r.classList.remove('expanded'));
        // Toggle this one
        if (!isExpanded) {
          detailRow.classList.add('expanded');
          row.classList.add('expanded');
        }
      });
    });
  }

  formatHexSpaced(hex) {
    if (!hex) return '';
    return hex.match(/.{1,2}/g).join(' ');
  }

  formatDataForMode(hex) {
    if (!hex) return '';
    const bytes = hex.match(/.{1,2}/g);
    if (!bytes) return '';

    switch (this.statsDecodeMode) {
      case 'ascii':
        return bytes.map(b => {
          const code = parseInt(b, 16);
          return (code >= 0x20 && code <= 0x7E) ? String.fromCharCode(code) : '.';
        }).join('');
      case 'dec-byte':
        return bytes.map(b => parseInt(b, 16)).join(' ');
      case 'dec-16le':
        const words = [];
        for (let i = 0; i < bytes.length; i += 2) {
          const lo = parseInt(bytes[i], 16);
          if (i + 1 < bytes.length) {
            const hi = parseInt(bytes[i + 1], 16);
            words.push((hi << 8) | lo);
          } else {
            words.push(lo);
          }
        }
        return words.join(' ');
      case 'hex':
      default:
        return bytes.join(' ');
    }
  }

  // Send CAN message via developer tools
  sendCANMessage() {
    if (!this.devTools) {
      this.showToast("DevTools not available", "error");
      return;
    }

    const idInput = document.getElementById("devMsgId");
    const dlcInput = document.getElementById("devMsgDlc");
    const dataInput = document.getElementById("devMsgData");
    const statusEl = document.getElementById("devSendStatus");

    try {
      const id = idInput.value.trim();
      const dlc = parseInt(dlcInput.value);
      const data = dataInput.value.trim();

      // Check extended checkbox
      const extCheckbox = document.getElementById("devMsgExtended");
      const extended = extCheckbox ? extCheckbox.checked : undefined;

      // Send message (pass extended override if checkbox exists)
      const msg = this.devTools.sendCANMessage(id, dlc, data, extended ? true : undefined);

      // Show success status with appropriate ID padding
      const idPad = msg.extended ? 8 : 3;
      const idHex = msg.id.toString(16).toUpperCase().padStart(idPad, "0");
      const extLabel = msg.extended ? " [EXT]" : "";
      statusEl.textContent = `Sent: ID=0x${idHex}${extLabel} DLC=${msg.dlc}`;
      statusEl.className = "dev-status success";

      setTimeout(() => {
        statusEl.className = "dev-status";
      }, 3000);

      this.showToast("Message sent successfully", "success");
    } catch (error) {
      // Show error status
      statusEl.textContent = `✗ Error: ${error.message}`;
      statusEl.className = "dev-status error";

      this.showToast(`Send failed: ${error.message}`, "error");
    }
  }

  // Send batch of CAN messages
  async sendBatchMessages() {
    if (!this.devTools) {
      this.showToast("DevTools not available", "error");
      return;
    }

    const batchInput = document.getElementById("devBatchMessages");
    const intervalInput = document.getElementById("devBatchInterval");
    const statusEl = document.getElementById("devBatchStatus");

    if (!batchInput.value.trim()) {
      statusEl.textContent = "⚠ No messages to send";
      statusEl.className = "dev-status warning";
      return;
    }

    try {
      // Parse batch messages
      const messages = this.devTools.parseBatchMessages(batchInput.value);
      const interval = parseInt(intervalInput.value) || 0;

      statusEl.textContent = `Sending ${messages.length} message(s)...`;
      statusEl.className = "dev-status info";

      // Send with progress callback
      await this.devTools.sendBatchMessages(messages, interval, (current, total, msg) => {
        const idPad = msg.extended ? 8 : 3;
        const idHex = msg.idStr.replace(/^0x/, '').toUpperCase().padStart(idPad, '0');
        statusEl.textContent = `Sent ${current}/${total}: ID=0x${idHex} DLC=${msg.dlc}`;
      });

      statusEl.textContent = `✓ Successfully sent ${messages.length} message(s)`;
      statusEl.className = "dev-status success";
      this.showToast(`Batch sent: ${messages.length} messages`, "success");

      setTimeout(() => {
        statusEl.className = "dev-status";
      }, 4000);
    } catch (error) {
      statusEl.textContent = `✗ Error: ${error.message}`;
      statusEl.className = "dev-status error";
      this.showToast(`Batch send failed: ${error.message}`, "error");
    }
  }

  // Toggle send history visibility
  toggleSendHistory() {
    const histSection = document.getElementById("devHistorySection");
    if (!histSection) return;

    if (histSection.style.display === "none") {
      histSection.style.display = "block";
      this.updateSendHistory();
    } else {
      histSection.style.display = "none";
    }
  }

  // Update send history display
  updateSendHistory() {
    if (!this.devTools) return;

    const histList = document.getElementById("devHistoryList");
    if (!histList) return;

    const history = this.devTools.getHistory();

    if (history.length === 0) {
      histList.innerHTML =
        '<div style="color: var(--text-muted); padding: 1rem; text-align: center;">No messages sent yet</div>';
      return;
    }

    histList.innerHTML = history
      .map((msg, index) => {
        const formatted = this.devTools.formatMessage(msg);
        return `
        <div class="dev-history-item" data-index="${index}">
          <div class="history-msg">
            ID:${formatted.id} DLC:${formatted.dlc} Data:${formatted.data}
          </div>
          <div class="history-time">${formatted.timestamp}</div>
        </div>
      `;
      })
      .join("");

    // Add click handlers to resend
    histList.querySelectorAll(".dev-history-item").forEach((item) => {
      item.addEventListener("click", () => {
        const index = parseInt(item.dataset.index);
        this.resendFromHistory(index);
      });
    });
  }

  // Resend message from history
  resendFromHistory(index) {
    if (!this.devTools) return;

    try {
      this.devTools.resendFromHistory(index);
      this.showToast("Message resent from history", "success");
    } catch (error) {
      this.showToast(`Resend failed: ${error.message}`, "error");
    }
  }

  // Clear send history
  clearSendHistory() {
    if (!this.devTools) return;

    if (confirm("Clear all send history?")) {
      this.devTools.clearHistory();
      this.updateSendHistory();
      this.showToast("Send history cleared", "success");
    }
  }
}

// Initialize app when DOM is ready
document.addEventListener("DOMContentLoaded", () => {
  window.app = new BatteryMonitor();
});
