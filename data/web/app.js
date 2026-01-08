// eBike Battery Monitor - Web App
// Mobile-optimized with WebSocket support

class BatteryMonitor {
    constructor() {
        this.ws = null;
        this.reconnectInterval = null;
        this.config = {};
        this.batteries = [];

        this.init();
    }

    init() {
        this.setupEventListeners();
        this.loadConfig();
        this.connectWebSocket();
        this.startPeriodicUpdates();
    }

    setupEventListeners() {
        // Settings button
        document.getElementById('settingsBtn').addEventListener('click', () => {
            this.showModal();
        });

        // Close settings
        document.getElementById('closeSettings').addEventListener('click', () => {
            this.hideModal();
        });

        // Click outside modal to close
        document.getElementById('settingsModal').addEventListener('click', (e) => {
            if (e.target.id === 'settingsModal') {
                this.hideModal();
            }
        });

        // WiFi form
        document.getElementById('wifiForm').addEventListener('submit', (e) => {
            e.preventDefault();
            this.saveWiFiConfig();
        });

        // MQTT form
        document.getElementById('mqttForm').addEventListener('submit', (e) => {
            e.preventDefault();
            this.saveMQTTConfig();
        });

        // Reboot button
        document.getElementById('rebootBtn').addEventListener('click', () => {
            if (confirm('Are you sure you want to reboot the device?')) {
                this.rebootDevice();
            }
        });

        // Clear WiFi button
        document.getElementById('clearWiFiBtn').addEventListener('click', () => {
            if (confirm('This will clear WiFi settings and reboot. The device will start in AP mode. Continue?')) {
                this.clearWiFi();
            }
        });
    }

    // WebSocket Management
    connectWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws`;

        console.log('Connecting to WebSocket:', wsUrl);

        try {
            this.ws = new WebSocket(wsUrl);

            this.ws.onopen = () => {
                console.log('WebSocket connected');
                this.showToast('Connected', 'success');
                this.clearReconnectInterval();
            };

            this.ws.onmessage = (event) => {
                this.handleWebSocketMessage(event.data);
            };

            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
            };

            this.ws.onclose = () => {
                console.log('WebSocket disconnected');
                this.showToast('Disconnected - Reconnecting...', 'warning');
                this.scheduleReconnect();
            };
        } catch (error) {
            console.error('Failed to create WebSocket:', error);
            this.scheduleReconnect();
        }
    }

    handleWebSocketMessage(data) {
        try {
            const message = JSON.parse(data);

            switch (message.type) {
                case 'battery_update':
                    this.updateBatteries(message.data);
                    break;
                case 'system_status':
                    this.updateSystemStatus(message.data);
                    break;
                case 'can_message':
                    // Could display CAN messages in real-time if needed
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
            console.error('Error parsing WebSocket message:', error);
        }
    }

    scheduleReconnect() {
        if (this.reconnectInterval) return;

        this.reconnectInterval = setInterval(() => {
            console.log('Attempting to reconnect...');
            this.connectWebSocket();
        }, 5000);
    }

    clearReconnectInterval() {
        if (this.reconnectInterval) {
            clearInterval(this.reconnectInterval);
            this.reconnectInterval = null;
        }
    }

    // UI Updates
    updateBatteries(data) {
        this.batteries = data.batteries || [];

        // Update summary
        document.getElementById('totalPower').textContent =
            (data.total_power || 0).toFixed(1) + ' W';
        document.getElementById('totalCurrent').textContent =
            (data.total_current || 0).toFixed(2) + ' A';
        document.getElementById('avgVoltage').textContent =
            (data.average_voltage || 0).toFixed(1) + ' V';

        // Update or create battery cards
        const container = document.getElementById('batteriesContainer');

        if (this.batteries.length === 0) {
            container.innerHTML = '<div class="loading">No batteries configured</div>';
            return;
        }

        // Clear existing cards
        container.innerHTML = '';

        // Create cards for each battery
        this.batteries.forEach(battery => {
            const card = this.createBatteryCard(battery);
            container.appendChild(card);
        });
    }

    createBatteryCard(battery) {
        const card = document.createElement('div');
        card.className = 'battery-card';
        if (battery.has_error) {
            card.classList.add('error');
        }

        const statusClass = battery.has_error ? 'error' : 'ok';
        const statusText = battery.has_error ? 'Error' : 'OK';

        card.innerHTML = `
            <div class="battery-header">
                <div class="battery-name">${this.escapeHtml(battery.name || `Battery ${battery.id}`)}</div>
                <div class="battery-status ${statusClass}">${statusText}</div>
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
                    <span class="metric-value">${battery.soc || 0} <span class="metric-unit">%</span></span>
                </div>
            </div>
        `;

        return card;
    }

    updateSystemStatus(data) {
        // WiFi status
        const wifiConnected = data.wifi_connected || false;
        const wifiStatusEl = document.getElementById('wifiStatus');
        if (wifiConnected) {
            wifiStatusEl.textContent = data.wifi_ssid || 'Connected';
            wifiStatusEl.style.color = 'var(--success-color)';
        } else {
            wifiStatusEl.textContent = 'AP Mode';
            wifiStatusEl.style.color = 'var(--warning-color)';
        }

        // IP address
        document.getElementById('ipAddress').textContent = data.wifi_ip || '-';

        // Uptime
        if (data.uptime_ms) {
            document.getElementById('uptime').textContent = this.formatUptime(data.uptime_ms);
        }

        // System info in settings
        if (data.chip_model) {
            document.getElementById('chipModel').textContent = data.chip_model;
        }
        if (data.cpu_freq_mhz) {
            document.getElementById('cpuFreq').textContent = data.cpu_freq_mhz + ' MHz';
        }
        if (data.free_heap) {
            document.getElementById('freeHeap').textContent =
                (data.free_heap / 1024).toFixed(1) + ' KB';
        }
        if (data.sdk_version) {
            document.getElementById('sdkVersion').textContent = data.sdk_version;
        }
    }

    // API Calls
    async loadConfig() {
        try {
            const response = await fetch('/api/config');
            if (response.ok) {
                this.config = await response.json();
                this.populateConfigForm();
            }
        } catch (error) {
            console.error('Error loading config:', error);
        }
    }

    populateConfigForm() {
        // WiFi
        document.getElementById('wifiSSID').value = this.config.wifi_ssid || '';

        // MQTT
        document.getElementById('mqttBroker').value = this.config.mqtt_broker || '';
        document.getElementById('mqttPort').value = this.config.mqtt_port || 1883;
        document.getElementById('mqttUsername').value = this.config.mqtt_username || '';
        document.getElementById('mqttTopicPrefix').value = this.config.mqtt_topic_prefix || 'ebike';
    }

    async saveWiFiConfig() {
        const ssid = document.getElementById('wifiSSID').value.trim();
        const password = document.getElementById('wifiPassword').value;

        if (!ssid) {
            this.showToast('SSID is required', 'error');
            return;
        }

        const config = {
            wifi_ssid: ssid
        };

        // Only include password if it's not empty
        if (password) {
            config.wifi_password = password;
        }

        try {
            const response = await fetch('/api/config', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(config)
            });

            const result = await response.json();

            if (response.ok && result.success) {
                this.showToast('WiFi settings saved! Rebooting to apply...', 'success');

                // Clear password field
                document.getElementById('wifiPassword').value = '';

                // Reboot after 2 seconds
                setTimeout(() => {
                    this.rebootDevice();
                }, 2000);
            } else {
                this.showToast(result.message || 'Failed to save WiFi settings', 'error');
            }
        } catch (error) {
            console.error('Error saving WiFi config:', error);
            this.showToast('Network error - check connection', 'error');
        }
    }

    async saveMQTTConfig() {
        const config = {
            mqtt_broker: document.getElementById('mqttBroker').value.trim(),
            mqtt_port: parseInt(document.getElementById('mqttPort').value) || 1883,
            mqtt_username: document.getElementById('mqttUsername').value.trim(),
            mqtt_password: document.getElementById('mqttPassword').value,
            mqtt_topic_prefix: document.getElementById('mqttTopicPrefix').value.trim() || 'ebike'
        };

        try {
            const response = await fetch('/api/config', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(config)
            });

            const result = await response.json();

            if (response.ok && result.success) {
                this.showToast('MQTT settings saved!', 'success');
                document.getElementById('mqttPassword').value = '';
            } else {
                this.showToast(result.message || 'Failed to save MQTT settings', 'error');
            }
        } catch (error) {
            console.error('Error saving MQTT config:', error);
            this.showToast('Network error - check connection', 'error');
        }
    }

    async rebootDevice() {
        try {
            await fetch('/api/reset', {
                method: 'POST'
            });

            this.showToast('Device rebooting...', 'warning');
            this.hideModal();

            // Close WebSocket
            if (this.ws) {
                this.ws.close();
            }

            // Show reconnecting message
            setTimeout(() => {
                this.showToast('Waiting for device to restart...', 'warning');
            }, 3000);

        } catch (error) {
            console.error('Error rebooting device:', error);
        }
    }

    async clearWiFi() {
        // This would need to be implemented as a specific API endpoint
        // For now, we'll just show a message
        this.showToast('Please use serial command: reset_wifi', 'warning');
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
            const response = await fetch('/api/status');
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
            console.error('Error fetching status:', error);
        }
    }

    // UI Helpers
    showModal() {
        document.getElementById('settingsModal').classList.add('active');
        // Reload config when opening settings
        this.loadConfig();
    }

    hideModal() {
        document.getElementById('settingsModal').classList.remove('active');
    }

    showToast(message, type = 'info') {
        const toast = document.getElementById('toast');
        toast.textContent = message;
        toast.className = 'toast show ' + type;

        setTimeout(() => {
            toast.classList.remove('show');
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

    escapeHtml(text) {
        const map = {
            '&': '&amp;',
            '<': '&lt;',
            '>': '&gt;',
            '"': '&quot;',
            "'": '&#039;'
        };
        return text.replace(/[&<>"']/g, m => map[m]);
    }
}

// Initialize app when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    window.app = new BatteryMonitor();
});
