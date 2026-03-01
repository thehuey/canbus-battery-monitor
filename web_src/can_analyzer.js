// CAN Message Analyzer - IndexedDB + Statistics + ASCII Detection
// Vanilla JS, compact and efficient

class CANAnalyzer {
  constructor() {
    this.db = null;
    this.dbName = 'ebike_can_analyzer';
    this.dbVersion = 2;
    this.messageStats = new Map(); // In-memory cache: id -> stats
    this.maxUniqueData = 100; // Max unique payloads per message ID
    this.saveInterval = null;
    this.dirty = new Set(); // IDs that need saving
    this.ready = this.init();
  }

  async init() {
    await this.openDatabase();
    await this.loadStats();
    // Periodic save every 5 seconds for dirty entries
    this.saveInterval = setInterval(() => this.flushDirty(), 5000);
  }

  openDatabase() {
    return new Promise((resolve, reject) => {
      const request = indexedDB.open(this.dbName, this.dbVersion);
      request.onerror = () => reject(request.error);
      request.onsuccess = () => { this.db = request.result; resolve(); };

      request.onupgradeneeded = (event) => {
        const db = event.target.result;

        // Delete old stores from v1 if they exist
        for (const name of ['message_stats', 'byte_patterns', 'annotations', 'protocol']) {
          if (db.objectStoreNames.contains(name)) {
            db.deleteObjectStore(name);
          }
        }

        // v2 stores
        db.createObjectStore('msg_stats', { keyPath: 'id' });
        db.createObjectStore('annotations', { keyPath: 'id' });
      };
    });
  }

  async loadStats() {
    if (!this.db) return;
    return new Promise((resolve) => {
      const tx = this.db.transaction(['msg_stats'], 'readonly');
      const req = tx.objectStore('msg_stats').getAll();
      req.onsuccess = () => {
        for (const stat of req.result) {
          // Reconstruct uniqueData map from array
          const dataMap = new Map();
          if (stat.uniqueData) {
            for (const entry of stat.uniqueData) {
              dataMap.set(entry.hex, entry);
            }
          }
          stat._dataMap = dataMap;
          this.messageStats.set(stat.id, stat);
        }
        console.debug(`[CANAnalyzer] Loaded ${req.result.length} message stats`);
        resolve();
      };
      req.onerror = () => resolve();
    });
  }

  // Core analysis - called for every CAN message
  analyzeMessage(canMsg) {
    const id = canMsg.id;
    const dataHex = canMsg.data;
    const now = Date.now();

    let stats = this.messageStats.get(id);
    if (!stats) {
      stats = {
        id,
        count: 0,
        firstSeen: now,
        lastSeen: now,
        dlc: canMsg.dlc,
        avgInterval: 0,
        lastTimestamp: 0,
        hasASCII: false,
        asciiDecodeAlways: false,
        _dataMap: new Map()
      };
      this.messageStats.set(id, stats);
    }

    stats.count++;
    stats.lastSeen = now;
    stats.dlc = canMsg.dlc;

    // Interval tracking
    if (stats.lastTimestamp > 0) {
      const interval = now - stats.lastTimestamp;
      if (interval > 0 && interval < 60000) { // Ignore gaps > 60s
        stats.avgInterval = stats.avgInterval === 0
          ? interval
          : 0.9 * stats.avgInterval + 0.1 * interval;
      }
    }
    stats.lastTimestamp = now;

    // Track unique complete data payloads
    let dataEntry = stats._dataMap.get(dataHex);
    if (!dataEntry) {
      // Check ASCII for new unique payloads
      const ascii = this.tryASCII(dataHex, canMsg.dlc);

      dataEntry = {
        hex: dataHex,
        count: 0,
        firstSeen: now,
        lastSeen: now,
        ascii: ascii
      };
      stats._dataMap.set(dataHex, dataEntry);

      if (ascii) {
        stats.hasASCII = true;
      }

      // Evict oldest if over limit
      if (stats._dataMap.size > this.maxUniqueData) {
        // Remove entry with oldest lastSeen
        let oldestKey = null, oldestTime = Infinity;
        for (const [key, val] of stats._dataMap) {
          if (val.lastSeen < oldestTime) {
            oldestTime = val.lastSeen;
            oldestKey = key;
          }
        }
        if (oldestKey) stats._dataMap.delete(oldestKey);
      }
    }
    dataEntry.count++;
    dataEntry.lastSeen = now;

    this.dirty.add(id);
    return stats;
  }

  // Try to decode hex data as ASCII. Returns string or null.
  tryASCII(hex, dlc) {
    if (!hex || dlc === 0) return null;

    const bytes = [];
    for (let i = 0; i < hex.length; i += 2) {
      bytes.push(parseInt(hex.substr(i, 2), 16));
    }

    let printable = 0;
    const chars = [];
    for (let i = 0; i < bytes.length; i++) {
      const b = bytes[i];
      if (b === 0) break; // null terminator
      if (b >= 32 && b <= 126) {
        printable++;
        chars.push(String.fromCharCode(b));
      } else {
        chars.push('.');
      }
    }

    // Need at least 3 printable chars AND >60% printable
    if (printable >= 3 && chars.length > 0 && (printable / chars.length) >= 0.6) {
      return chars.join('');
    }
    return null;
  }

  // Get stats for one ID
  getStats(id) {
    return this.messageStats.get(id);
  }

  // Get all stats sorted by count (descending)
  getAllStats() {
    return Array.from(this.messageStats.values())
      .sort((a, b) => b.count - a.count);
  }

  // Get unique data entries for a message ID, sorted by count
  getUniqueData(id) {
    const stats = this.messageStats.get(id);
    if (!stats) return [];
    return Array.from(stats._dataMap.values())
      .sort((a, b) => b.count - a.count);
  }

  // Set user annotation for a message ID
  async setAnnotation(id, data) {
    if (!this.db) return;
    const existing = await this.getAnnotation(id) || { id };
    const merged = { ...existing, ...data, updatedAt: Date.now() };
    const tx = this.db.transaction(['annotations'], 'readwrite');
    tx.objectStore('annotations').put(merged);
  }

  async getAnnotation(id) {
    if (!this.db) return null;
    return new Promise((resolve) => {
      const tx = this.db.transaction(['annotations'], 'readonly');
      const req = tx.objectStore('annotations').get(id);
      req.onsuccess = () => resolve(req.result);
      req.onerror = () => resolve(null);
    });
  }

  async getAllAnnotations() {
    if (!this.db) return [];
    return new Promise((resolve) => {
      const tx = this.db.transaction(['annotations'], 'readonly');
      const req = tx.objectStore('annotations').getAll();
      req.onsuccess = () => resolve(req.result);
      req.onerror = () => resolve([]);
    });
  }

  // Flush dirty entries to IndexedDB
  async flushDirty() {
    if (!this.db || this.dirty.size === 0) return;

    const ids = Array.from(this.dirty);
    this.dirty.clear();

    const tx = this.db.transaction(['msg_stats'], 'readwrite');
    const store = tx.objectStore('msg_stats');

    for (const id of ids) {
      const stats = this.messageStats.get(id);
      if (!stats) continue;

      // Serialize for storage (convert Map to Array, strip internal fields)
      const toSave = {
        id: stats.id,
        count: stats.count,
        firstSeen: stats.firstSeen,
        lastSeen: stats.lastSeen,
        dlc: stats.dlc,
        avgInterval: stats.avgInterval,
        lastTimestamp: stats.lastTimestamp,
        hasASCII: stats.hasASCII,
        asciiDecodeAlways: stats.asciiDecodeAlways,
        uniqueData: Array.from(stats._dataMap.values())
      };
      store.put(toSave);
    }
  }

  // Force save all stats now
  async saveAll() {
    for (const id of this.messageStats.keys()) {
      this.dirty.add(id);
    }
    await this.flushDirty();
  }

  // Export all data as JSON
  async exportData() {
    await this.saveAll();
    if (!this.db) return null;

    const data = { version: this.dbVersion, exportedAt: Date.now() };

    const [stats, annotations] = await Promise.all([
      new Promise(r => {
        const req = this.db.transaction(['msg_stats'], 'readonly')
          .objectStore('msg_stats').getAll();
        req.onsuccess = () => r(req.result);
        req.onerror = () => r([]);
      }),
      new Promise(r => {
        const req = this.db.transaction(['annotations'], 'readonly')
          .objectStore('annotations').getAll();
        req.onsuccess = () => r(req.result);
        req.onerror = () => r([]);
      })
    ]);

    data.messageStats = stats;
    data.annotations = annotations;
    return data;
  }

  // Import data from JSON
  async importData(data) {
    if (!this.db || !data) return false;
    try {
      if (data.messageStats) {
        const tx = this.db.transaction(['msg_stats'], 'readwrite');
        const store = tx.objectStore('msg_stats');
        for (const stat of data.messageStats) store.put(stat);
      }
      if (data.annotations) {
        const tx = this.db.transaction(['annotations'], 'readwrite');
        const store = tx.objectStore('annotations');
        for (const ann of data.annotations) store.put(ann);
      }
      await this.loadStats();
      return true;
    } catch (e) {
      console.error('[CANAnalyzer] Import failed:', e);
      return false;
    }
  }

  // Clear all data
  async clearAllData() {
    if (!this.db) return false;
    for (const name of ['msg_stats', 'annotations']) {
      const tx = this.db.transaction([name], 'readwrite');
      await new Promise(r => {
        const req = tx.objectStore(name).clear();
        req.onsuccess = () => r();
        req.onerror = () => r();
      });
    }
    this.messageStats.clear();
    this.dirty.clear();
    return true;
  }

  destroy() {
    if (this.saveInterval) clearInterval(this.saveInterval);
    this.flushDirty();
  }
}

window.CANAnalyzer = CANAnalyzer;
