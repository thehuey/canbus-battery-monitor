// Protocol Manager - Load and parse CAN messages using protocol definitions
// Runs entirely in the browser

class ProtocolManager {
  constructor() {
    this.protocols = {};          // Cache of loaded protocols
    this.activeProtocol = null;   // Currently selected protocol
    this.ready = this.init();
  }

  async init() {
    // Load available protocols from the server
    try {
      const response = await fetch('/api/protocols');
      const data = await response.json();

      console.debug('[ProtocolManager] Available protocols:', data);

      // Load built-in protocols
      if (data.builtin) {
        for (const proto of data.builtin) {
          const protoData = await this.loadProtocol(`builtin_${proto.id}`);
          if (protoData) {
            this.protocols[`builtin_${proto.id}`] = protoData;
          }
        }
      }

      // Load custom protocols if any
      if (data.custom) {
        for (const proto of data.custom) {
          const protoData = await this.loadProtocol(proto.filename);
          if (protoData) {
            this.protocols[proto.filename] = protoData;
          }
        }
      }

      // Set default protocol to first built-in
      if (data.builtin && data.builtin.length > 0) {
        this.setActiveProtocol(`builtin_0`);
      }

      console.debug('[ProtocolManager] Initialized with protocols:', Object.keys(this.protocols));
      return true;
    } catch (error) {
      console.error('[ProtocolManager] Failed to load protocols:', error);
      return false;
    }
  }

  async loadProtocol(protocolId) {
    try {
      const response = await fetch(`/api/protocols/${protocolId}`);
      if (!response.ok) {
        console.warn(`[ProtocolManager] Failed to load protocol ${protocolId}`);
        return null;
      }

      const proto = await response.json();

      // Index messages by CAN ID for fast lookup
      proto.messageMap = {};
      if (proto.messages) {
        for (const msg of proto.messages) {
          proto.messageMap[msg.can_id] = msg;
        }
      }

      return proto;
    } catch (error) {
      console.error(`[ProtocolManager] Error loading protocol ${protocolId}:`, error);
      return null;
    }
  }

  setActiveProtocol(protocolId) {
    if (this.protocols[protocolId]) {
      this.activeProtocol = this.protocols[protocolId];
      console.debug(`[ProtocolManager] Active protocol set to: ${protocolId} (${this.activeProtocol.name})`);
      return true;
    } else {
      console.warn(`[ProtocolManager] Protocol not found: ${protocolId}`);
      return false;
    }
  }

  getAvailableProtocols() {
    return Object.entries(this.protocols).map(([id, proto]) => ({
      id: id,
      name: proto.name,
      manufacturer: proto.manufacturer,
      version: proto.version
    }));
  }

  parseMessage(canMessage) {
    if (!this.activeProtocol) {
      return null;
    }

    const msgDef = this.activeProtocol.messageMap[canMessage.id];
    if (!msgDef) {
      return null;  // Message not in protocol
    }

    const result = {
      can_id: canMessage.id,
      message_name: msgDef.name,
      description: msgDef.description,
      fields: {}
    };

    // Extract fields from the CAN data
    if (msgDef.fields) {
      for (const field of msgDef.fields) {
        const value = this.extractField(canMessage.data, field);
        if (value !== null) {
          result.fields[field.name] = {
            value: value,
            unit: field.unit,
            description: field.description
          };
        }
      }
    }

    return result;
  }

  extractField(data, field) {
    try {
      const offset = field.byte_offset;
      const length = field.length;

      if (offset + length > data.length) {
        console.warn(`[ProtocolManager] Field ${field.name} exceeds data length`);
        return null;
      }

      let rawValue = 0;

      // Parse based on data type
      switch (field.data_type) {
        case 'uint8':
          rawValue = data[offset];
          break;
        case 'uint16_le':
          rawValue = data[offset] | (data[offset + 1] << 8);
          break;
        case 'uint16_be':
          rawValue = (data[offset] << 8) | data[offset + 1];
          break;
        case 'uint32_le':
          rawValue = data[offset] |
                     (data[offset + 1] << 8) |
                     (data[offset + 2] << 16) |
                     (data[offset + 3] << 24);
          break;
        case 'int8':
          rawValue = data[offset];
          if (rawValue & 0x80) rawValue = rawValue - 256;
          break;
        case 'int16_le':
          rawValue = data[offset] | (data[offset + 1] << 8);
          if (rawValue & 0x8000) rawValue = rawValue - 65536;
          break;
        default:
          console.warn(`[ProtocolManager] Unknown data type: ${field.data_type}`);
          return null;
      }

      // Apply scale and offset
      let value = rawValue * (field.scale || 1.0) + (field.offset || 0.0);

      // Round to reasonable precision
      value = Math.round(value * 100000) / 100000;

      return value;
    } catch (error) {
      console.error(`[ProtocolManager] Error extracting field ${field.name}:`, error);
      return null;
    }
  }

  // Map protocol fields to standard battery data fields
  extractBatteryData(canMessage) {
    const parsed = this.parseMessage(canMessage);
    if (!parsed) {
      return null;
    }

    const batteryData = {
      can_id: parsed.can_id,
      message_name: parsed.message_name
    };

    // Map common field names to battery data structure
    for (const [fieldName, fieldValue] of Object.entries(parsed.fields)) {
      const value = fieldValue.value;
      const unit = fieldValue.unit;

      // Voltage fields
      if (fieldName.includes('voltage') && fieldName.includes('pack')) {
        if (unit === 'mV') {
          batteryData.pack_voltage = value / 1000;
        } else {
          batteryData.pack_voltage = value;
        }
      }
      // Current fields
      else if (fieldName.includes('current') && fieldName.includes('pack')) {
        if (unit === 'mA') {
          batteryData.pack_current = value / 1000;
        } else {
          batteryData.pack_current = value;
        }
      }
      // SOC
      else if (fieldName === 'soc' || fieldName.includes('state_of_charge')) {
        batteryData.soc = Math.round(value);
      }
      // Remaining capacity
      else if (fieldName.includes('remaining') && fieldName.includes('mAh')) {
        batteryData.remaining_mah = value;
      }
      // Max capacity
      else if (fieldName.includes('max') && fieldName.includes('mAh')) {
        batteryData.max_mah = value;
      }
      // Temperature
      else if (fieldName === 'temperature' || fieldName === 'temp1' || fieldName === 'temp_1') {
        batteryData.temp1 = value;
      }
      else if (fieldName === 'temp2' || fieldName === 'temp_2') {
        batteryData.temp2 = value;
      }
      // State/Status
      else if (fieldName === 'state' || fieldName === 'status' || fieldName === 'status_flags') {
        batteryData.status_flags = Math.round(value);
      }
      // Pack identifier
      else if (fieldName === 'pack_identifier') {
        batteryData.pack_identifier = Math.round(value);
      }
    }

    return batteryData;
  }
}

// Export to global scope
window.ProtocolManager = ProtocolManager;
