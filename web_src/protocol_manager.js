// Protocol Manager - Load and parse CAN messages using protocol definitions
// Runs entirely in the browser

class ProtocolManager {
  constructor() {
    this.protocolList = []; // Metadata from /api/protocols (filename, name, manufacturer)
    this.protocolCache = {}; // Fully loaded protocol data (fetched on demand)
    this.activeProtocol = null; // Currently selected & loaded protocol
    this.activeProtocolId = null; // Filename of active protocol
    this.batteryState = {}; // Accumulated battery data from parsed CAN messages
    this.onBatteryUpdate = null; // Callback when battery data changes
    this._cellVoltageIndex = 0; // Track sequential cell voltage messages
    this.ready = this.init();
  }

  async init() {
    // Fetch protocol listing (includes name/manufacturer metadata from firmware)
    try {
      const response = await fetch("/api/protocols");
      const data = await response.json();

      if (data.custom) {
        this.protocolList = data.custom;
      }

      console.debug(
        "[ProtocolManager] Available protocols:",
        this.protocolList.map((p) => p.filename),
      );

      // Auto-activate first protocol
      if (this.protocolList.length > 0) {
        await this.setActiveProtocol(this.protocolList[0].filename);
      }

      return true;
    } catch (error) {
      console.error("[ProtocolManager] Failed to load protocol list:", error);
      return false;
    }
  }

  // Fetch and cache the full protocol JSON (lazy — only when needed for parsing)
  async fetchProtocol(protocolId) {
    if (this.protocolCache[protocolId]) {
      return this.protocolCache[protocolId];
    }

    try {
      const response = await fetch(`/protocols/${protocolId}`);
      if (!response.ok) {
        console.warn(
          `[ProtocolManager] Failed to fetch protocol ${protocolId}: ${response.status}`,
        );
        return null;
      }

      const proto = await response.json();

      // Index messages by numeric CAN ID for fast lookup
      proto.messageMap = {};
      if (proto.messages) {
        for (const msg of proto.messages) {
          proto.messageMap[msg.can_id] = msg;
        }
      }

      this.protocolCache[protocolId] = proto;
      console.debug(
        `[ProtocolManager] Loaded protocol: ${proto.name} (${Object.keys(proto.messageMap).length} messages)`,
      );
      return proto;
    } catch (error) {
      console.error(
        `[ProtocolManager] Error fetching protocol ${protocolId}:`,
        error,
      );
      return null;
    }
  }

  async setActiveProtocol(protocolId) {
    const proto = await this.fetchProtocol(protocolId);
    if (proto) {
      this.activeProtocol = proto;
      this.activeProtocolId = protocolId;
      this.batteryState = {}; // Reset accumulated state on protocol change
      this._cellVoltageIndex = 0; // Reset cell voltage tracking
      this._hasDirectPackVoltage = false; // Reset direct voltage flag
      console.debug(
        `[ProtocolManager] Active protocol set to: ${protocolId} (${proto.name})`,
      );
      return true;
    }
    console.warn(
      `[ProtocolManager] Protocol not found or failed to load: ${protocolId}`,
    );
    return false;
  }

  getAvailableProtocols() {
    return this.protocolList.map((p) => ({
      id: p.filename,
      name: p.name || p.filename.replace(".json", ""),
      manufacturer: p.manufacturer || "",
    }));
  }

  // Convert hex string CAN ID to numeric (e.g., "0x201" -> 513)
  canIdToNumber(idStr) {
    if (typeof idStr === "number") return idStr;
    return parseInt(idStr, 16);
  }

  // Convert hex data string to byte array (e.g., "A2000000" -> [0xA2, 0x00, 0x00, 0x00])
  hexToBytes(hexStr) {
    const bytes = [];
    const clean = hexStr.replace(/\s/g, "");
    for (let i = 0; i < clean.length; i += 2) {
      bytes.push(parseInt(clean.substr(i, 2), 16));
    }
    return bytes;
  }

  // Process an incoming CAN message from the WebSocket
  // canMessage: { id: "0x203", dlc: 8, data: "A200E70E..." }
  processCANMessage(canMessage) {
    if (!this.activeProtocol) return null;

    const numericId = this.canIdToNumber(canMessage.id);
    const msgDef = this.activeProtocol.messageMap[numericId];
    if (!msgDef) return null; // Not in protocol

    const dataBytes = this.hexToBytes(canMessage.data);
    const parsed = this.parseFields(msgDef, dataBytes);
    if (!parsed) return null;

    // Accumulate into battery state
    this.applyToBatteryState(msgDef, parsed);

    // Notify listener
    if (this.onBatteryUpdate) {
      this.onBatteryUpdate(this.batteryState);
    }

    return parsed;
  }

  parseFields(msgDef, dataBytes) {
    if (!msgDef.fields) return null;

    const result = {
      can_id: msgDef.can_id,
      message_name: msgDef.name,
      fields: {},
    };

    for (const field of msgDef.fields) {
      const value = this.extractField(dataBytes, field);
      if (value === null) continue;

      // Resolve enum display name if available
      let displayValue = null;
      if (field.enum_values && typeof value === "number") {
        displayValue = field.enum_values[String(Math.round(value))] || null;
      }

      result.fields[field.name] = {
        value: value,
        displayValue: displayValue,
        unit: field.unit,
        description: field.description,
      };
    }

    return result;
  }

  extractField(data, field) {
    try {
      const offset = field.byte_offset;
      const length = field.length;

      if (offset + length > data.length) {
        return null;
      }

      // Handle ASCII type separately — returns a string
      if (field.data_type === "ascii") {
        let str = "";
        for (let i = offset; i < offset + length && i < data.length; i++) {
          if (data[i] >= 0x20 && data[i] <= 0x7e) {
            str += String.fromCharCode(data[i]);
          }
        }
        return str;
      }

      let rawValue = 0;

      switch (field.data_type) {
        case "uint8":
          rawValue = data[offset];
          break;
        case "uint16_le":
          rawValue = data[offset] | (data[offset + 1] << 8);
          break;
        case "uint16_be":
          rawValue = (data[offset] << 8) | data[offset + 1];
          break;
        case "uint32_le":
          rawValue =
            (data[offset] |
              (data[offset + 1] << 8) |
              (data[offset + 2] << 16) |
              ((data[offset + 3] << 24) >>> 0)) >>>
            0; // unsigned
          break;
        case "uint32_be":
          rawValue =
            ((data[offset] << 24) |
              (data[offset + 1] << 16) |
              (data[offset + 2] << 8) |
              data[offset + 3]) >>>
            0;
          break;
        case "int8":
          rawValue = data[offset];
          if (rawValue & 0x80) rawValue = rawValue - 256;
          break;
        case "int16_le":
          rawValue = data[offset] | (data[offset + 1] << 8);
          if (rawValue & 0x8000) rawValue = rawValue - 65536;
          break;
        case "int16_be":
          rawValue = (data[offset] << 8) | data[offset + 1];
          if (rawValue & 0x8000) rawValue = rawValue - 65536;
          break;
        default:
          return null;
      }

      // Apply scale and offset from protocol definition
      let value = rawValue * (field.scale || 1.0) + (field.offset || 0.0);

      // Round to reasonable precision
      value = Math.round(value * 100000) / 100000;

      return value;
    } catch (error) {
      console.error(
        `[ProtocolManager] Error extracting field ${field.name}:`,
        error,
      );
      return null;
    }
  }

  // Map parsed fields into the accumulated battery state object
  applyToBatteryState(msgDef, parsed) {
    for (const [fieldName, fieldData] of Object.entries(parsed.fields)) {
      const value = fieldData.value;
      const unit = fieldData.unit;
      const lowerName = fieldName.toLowerCase();

      console.log(`processing field, fieldName: ${fieldName}`);

      // Single pack voltage in mV (e.g., dpower 0x202 pack_voltage_mv)
      if (lowerName === "pack_voltage_mv") {
        console.log("Direct pack voltage (mV):", value);
        this.batteryState.voltage = Math.round(value) / 1000;
        this._hasDirectPackVoltage = true;
        continue;
      }

      // Sequential cell voltage messages (same field name, one per CAN message)
      // Used by protocols that send one cell voltage per CAN frame across N frames
      if (lowerName === "cell_voltage_mv" || lowerName === "cell_voltage") {
        console.log("Received cell voltage (mV):", value);
        const cellCount = this.activeProtocol.cell_count || 13;
        if (!this.batteryState.cell_voltages_mv) {
          this.batteryState.cell_voltages_mv = new Array(cellCount).fill(0);
        }
        this.batteryState.cell_voltages_mv[this._cellVoltageIndex] = value;
        this._cellVoltageIndex = (this._cellVoltageIndex + 1) % cellCount;

        // Calculate pack voltage from cell sum when we have all cells
        // but only if no direct pack voltage has been set
        const cells = this.batteryState.cell_voltages_mv;

        if (!this._hasDirectPackVoltage && cells.every((v) => v > 0)) {
          this.batteryState.voltage =
            Math.round(cells.reduce((s, v) => s + v, 0)) / 1000;
        }
        continue;
      }

      // SOC / remaining capacity — distinguish mAh from percentage by unit
      if (
        lowerName === "soc" ||
        lowerName === "remaining_capacity" ||
        lowerName === "state_of_charge"
      ) {
        if (unit === "mAh") {
          this.batteryState.remaining_mah = value;
        } else {
          this.batteryState.soc = Math.round(value);
        }
        continue;
      }

      // Max capacity
      if (
        lowerName === "max_soc" ||
        lowerName === "max_capacity" ||
        lowerName === "full_capacity"
      ) {
        this.batteryState.max_mah = value;
        continue;
      }

      // Pack identifier
      if (lowerName === "pack_identifier") {
        this.batteryState.pack_identifier = Math.round(value);
        continue;
      }

      // BMS info (ascii string)
      if (lowerName === "bms_info") {
        this.batteryState.bms_info =
          typeof value === "string" ? value : String(value);
        continue;
      }

      // Battery state with enum display name
      if (lowerName === "state" || lowerName === "battery_state") {
        this.batteryState.state =
          typeof value === "number" ? Math.round(value) : value;
        if (fieldData.displayValue) {
          this.batteryState.state_name = fieldData.displayValue;
        }
        continue;
      }

      // Voltage (total pack)
      if (
        lowerName.includes("total_voltage") ||
        (lowerName.includes("voltage") && lowerName.includes("pack"))
      ) {
        this.batteryState.voltage = unit === "mV" ? value / 1000 : value;
        continue;
      }

      // Current
      if (
        lowerName.includes("current") &&
        (lowerName.includes("pack") || lowerName.includes("total"))
      ) {
        this.batteryState.current = unit === "mA" ? value / 1000 : value;
        continue;
      }

      // Temperature
      if (
        lowerName === "temperature" ||
        lowerName === "temp1" ||
        lowerName === "temp_1"
      ) {
        this.batteryState.temp1 = value;
      } else if (lowerName === "temp2" || lowerName === "temp_2") {
        this.batteryState.temp2 = value;
      }

      // Individual cell voltages with index in name (e.g., cell_voltage_1)
      if (
        lowerName.startsWith("cell_voltage_") &&
        lowerName !== "cell_voltage_mv"
      ) {
        if (!this.batteryState.cell_voltages) {
          this.batteryState.cell_voltages = {};
        }
        this.batteryState.cell_voltages[fieldName] =
          unit === "mV" ? value / 1000 : value;
      }

      // Average cell voltage
      if (
        lowerName.includes("avg_cell") ||
        lowerName.includes("average_cell")
      ) {
        this.batteryState.avg_cell_voltage =
          unit === "mV" ? value / 1000 : value;
      }
    }

    // Calculate SOC percentage from remaining/max mAh
    if (
      this.batteryState.remaining_mah !== undefined &&
      this.batteryState.max_mah !== undefined &&
      this.batteryState.max_mah > 0
    ) {
      this.batteryState.soc = Math.round(
        (this.batteryState.remaining_mah / this.batteryState.max_mah) * 100,
      );
    }

    // Calculate power from voltage and current
    if (
      this.batteryState.voltage !== undefined &&
      this.batteryState.current !== undefined
    ) {
      this.batteryState.power =
        Math.round(
          this.batteryState.voltage * this.batteryState.current * 100,
        ) / 100;
    }
  }

  // Get current battery state for display
  getBatteryState() {
    return { ...this.batteryState };
  }
}

// Export to global scope
window.ProtocolManager = ProtocolManager;
