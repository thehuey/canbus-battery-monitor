// Developer Tools - CAN message sending with extended frame support
// Supports standard (11-bit, 0x000-0x7FF) and extended (29-bit, 0x00000000-0x1FFFFFFF) CAN IDs

class DevTools {
  constructor() {
    this.ws = null;
    this.history = [];
    this.maxHistory = 50;
  }

  setWebSocket(ws) {
    this.ws = ws;
  }

  /**
   * Send a CAN message via WebSocket
   * @param {string} idStr - CAN ID as hex string (e.g. "0x100" or "0x1ABCDEF0")
   * @param {number} dlc - Data length code (0-8)
   * @param {string} dataStr - Data bytes as hex string (e.g. "01 02 03 04 05 06 07 08")
   * @param {boolean} [extendedOverride] - Force extended frame flag (auto-detected if undefined)
   * @returns {object} The sent message object
   */
  sendCANMessage(idStr, dlc, dataStr, extendedOverride) {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      throw new Error("WebSocket not connected");
    }

    // Parse CAN ID
    const id = parseInt(idStr, 16) || parseInt(idStr, 10);
    if (isNaN(id) || id < 0) {
      throw new Error("Invalid CAN ID");
    }

    // Determine if extended frame
    let extended;
    if (extendedOverride !== undefined) {
      extended = extendedOverride;
    } else {
      // Auto-detect: if ID > 0x7FF, it must be extended
      extended = id > 0x7FF;
    }

    // Validate ID range
    if (extended) {
      if (id > 0x1FFFFFFF) {
        throw new Error("Extended CAN ID must be 0x00000000-0x1FFFFFFF");
      }
    } else {
      if (id > 0x7FF) {
        throw new Error("Standard CAN ID must be 0x000-0x7FF (enable Extended for larger IDs)");
      }
    }

    // Validate DLC
    if (isNaN(dlc) || dlc < 0 || dlc > 8) {
      throw new Error("DLC must be 0-8");
    }

    // Parse data bytes
    const dataBytes = this.parseDataBytes(dataStr, dlc);

    // Build WebSocket command
    const cmd = {
      cmd: "can_send",
      id: id,
      dlc: dlc,
      data: Array.from(dataBytes),
      extended: extended,
    };

    this.ws.send(JSON.stringify(cmd));

    // Store in history
    const entry = {
      id: id,
      dlc: dlc,
      data: dataBytes,
      extended: extended,
      timestamp: Date.now(),
    };

    this.history.unshift(entry);
    if (this.history.length > this.maxHistory) {
      this.history.pop();
    }

    return entry;
  }

  /**
   * Parse hex data string into byte array
   * @param {string} dataStr - Hex bytes separated by spaces (e.g. "01 02 AB FF")
   * @param {number} dlc - Expected number of bytes
   * @returns {Uint8Array}
   */
  parseDataBytes(dataStr, dlc) {
    const bytes = new Uint8Array(dlc);

    if (!dataStr || dlc === 0) {
      return bytes;
    }

    // Remove common separators and parse hex pairs
    const cleaned = dataStr.replace(/[,\s]+/g, " ").trim();
    const parts = cleaned.split(" ").filter((s) => s.length > 0);

    for (let i = 0; i < Math.min(parts.length, dlc); i++) {
      const val = parseInt(parts[i], 16);
      if (isNaN(val) || val < 0 || val > 255) {
        throw new Error(`Invalid data byte at position ${i}: "${parts[i]}"`);
      }
      bytes[i] = val;
    }

    return bytes;
  }

  getHistory() {
    return this.history;
  }

  formatMessage(msg) {
    const idPad = msg.extended ? 8 : 3;
    const idStr = msg.id.toString(16).toUpperCase().padStart(idPad, "0");
    const dataHex = Array.from(msg.data)
      .map((b) => b.toString(16).toUpperCase().padStart(2, "0"))
      .join(" ");
    const time = new Date(msg.timestamp);
    const timeStr =
      time.toLocaleTimeString() +
      "." +
      time.getMilliseconds().toString().padStart(3, "0");

    return {
      id: (msg.extended ? "0x" : "0x") + idStr,
      dlc: msg.dlc,
      data: dataHex,
      extended: msg.extended,
      timestamp: timeStr,
    };
  }

  resendFromHistory(index) {
    if (index < 0 || index >= this.history.length) {
      throw new Error("Invalid history index");
    }

    const msg = this.history[index];
    const idStr = "0x" + msg.id.toString(16).toUpperCase();
    const dataStr = Array.from(msg.data)
      .map((b) => b.toString(16).toUpperCase().padStart(2, "0"))
      .join(" ");

    return this.sendCANMessage(idStr, msg.dlc, dataStr, msg.extended);
  }

  clearHistory() {
    this.history = [];
  }
}

// Export to global scope
window.DevTools = DevTools;
