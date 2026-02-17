# eBike CAN Monitor - New Web Features

## Overview

This update adds powerful CAN protocol analysis and developer tools to the web interface, running entirely in the browser using vanilla JavaScript and IndexedDB for persistence.

## Features

### 1. CAN Message Analyzer (IndexedDB)

The analyzer automatically tracks and analyzes all CAN messages in real-time, persisting data in the browser's IndexedDB.

**What it tracks:**
- Message frequency and timing patterns
- First and last seen timestamps
- Byte-by-byte statistics (min, max, changes, unique values)
- Average message interval (for detecting periodic messages)
- ASCII content detection

**How to use:**
- Analyzer runs automatically in the background
- Click the "Stats" button in the CAN Monitor to view statistics in the console
- Data persists across browser sessions

### 2. ASCII Detection

The analyzer automatically detects if CAN message data contains printable ASCII characters.

**Features:**
- Auto-detects ASCII content (70%+ printable characters)
- Shows toast notifications when ASCII is detected
- Stores detection results in IndexedDB
- Avoids repeatedly asking about the same message ID

**How it works:**
- When a message with ASCII content is detected, you'll see a toast notification
- The detected ASCII string is shown in the notification
- Check the browser console for detailed ASCII detection logs

### 3. Message Statistics Tracker

Tracks detailed statistics for each unique CAN message ID.

**Tracked metrics:**
- **Count**: Total messages received for this ID
- **First Seen**: Timestamp when first observed
- **Last Seen**: Timestamp when last observed
- **Avg Interval**: Average time between messages (in ms)
- **Byte Stats**: Per-byte min, max, changes, unique values

**Viewing stats:**
```javascript
// In browser console:
window.app.showCANStats();
```

This will output a formatted table showing all tracked message IDs and their statistics.

### 4. Developer Tools Panel

A new panel for composing and sending CAN messages to the device.

**Features:**
- **Send CAN Messages**: Compose custom CAN messages with ID, DLC, and data
- **Message History**: Automatically tracks last 50 sent messages
- **Resend from History**: Click any message in history to resend it
- **Multiple Data Formats**: Supports hex, decimal, or ASCII input

**How to use:**

1. **Send a message:**
   - Enter CAN ID (hex format, e.g., `0x123`)
   - Set DLC (0-8 bytes)
   - Enter data in hex format (e.g., `01 02 03 04 05 06 07 08`)
   - Click "Send Message"

2. **View send history:**
   - Click "History" button to toggle history display
   - Click any message in history to resend it

3. **Data format examples:**
   ```
   Hex (spaces):   01 02 03 04 05 06 07 08
   Hex (compact):  0102030405060708
   Decimal:        1,2,3,4,5,6,7,8
   ASCII:          Hello   (auto-converts to bytes)
   ```

4. **Clear history:**
   - Click "Clear History" button
   - History is saved to localStorage and persists across sessions

### 5. Data Backup & Restore

Export and import all analyzer data (message statistics, annotations, protocol discoveries).

**Export data:**
- Click "Export Data" button in Developer Tools
- Downloads a JSON file: `can_analysis_<timestamp>.json`
- Contains all IndexedDB data: message stats, annotations, protocol discoveries

**Import data:**
- Click "Import Data" button
- Select a previously exported JSON file
- Data is merged into IndexedDB (existing data is updated, new data is added)

**Use cases:**
- Share protocol discoveries with other users
- Backup analysis before clearing browser data
- Transfer analysis between different browsers/devices
- Archive learning sessions for different battery types

## Browser Console API

Advanced users can interact with the analyzer directly via the console:

```javascript
// Access the analyzer
const analyzer = window.app.canAnalyzer;

// Get stats for a specific message ID
const stats = analyzer.getStats('0x123');
console.log(stats);

// Get all message IDs sorted by frequency
const allStats = analyzer.getAllStats();
console.table(allStats);

// Export data programmatically
const data = await analyzer.exportData();
console.log(JSON.stringify(data, null, 2));

// Clear all data
await analyzer.clearAllData();

// Save annotation for a message ID
await analyzer.saveAnnotation('0x100', {
  name: 'Battery Status',
  description: 'Main battery pack status message',
  decodeAsASCII: false,
  notes: 'Bytes 0-1: voltage, 2-3: current'
});

// Get annotation
const annotation = await analyzer.getAnnotation('0x100');
console.log(annotation);
```

## Architecture

### Files Added

1. **can_analyzer.js** - IndexedDB + message statistics + ASCII detection
   - Compact vanilla JS, no dependencies
   - IndexedDB schema version 1
   - Stores: message_stats, byte_patterns, annotations, protocol

2. **dev_tools.js** - CAN message sender + send history
   - WebSocket message composition
   - Send history (localStorage)
   - Multiple data format parsing
   - Message validation

3. **FEATURES.md** - This documentation file

### Files Modified

1. **index.html**
   - Added script imports for can_analyzer.js and dev_tools.js
   - Added Developer Tools panel UI
   - Added Export/Import buttons
   - Added "Stats" button to CAN Monitor

2. **app.js**
   - Integrated CANAnalyzer and DevTools modules
   - Added WebSocket command handling for sending messages
   - Added event handlers for new UI elements
   - Added ASCII detection event listener
   - Added export/import/stats methods

3. **style.css**
   - Added styles for Developer Tools panel
   - Added styles for send history display
   - Mobile-responsive design for dev tools

4. **web_server.cpp** (ESP32 firmware)
   - Added handleWebSocketCommand() method
   - Handles "can_send" commands from browser
   - Validates and sends messages via CAN driver
   - Returns success/error responses

5. **web_server.h**
   - Added handleWebSocketCommand() declaration

## Data Persistence

### IndexedDB Stores

1. **message_stats**
   - Primary key: CAN message ID
   - Stores: count, firstSeen, lastSeen, avgInterval, byteStats

2. **byte_patterns** (future use)
   - For storing identified byte patterns and formulas

3. **annotations**
   - User notes, ASCII settings, message names
   - Fully customizable metadata per message ID

4. **protocol**
   - Protocol discoveries and documentation
   - Can be exported/shared as markdown

### localStorage Usage

- Developer Tools send history (last 50 messages)
- Survives browser restarts
- Cleared via "Clear History" button

## Security Considerations

- All data is stored locally in the browser (IndexedDB + localStorage)
- No data is sent to external servers
- Export files are plain JSON (human-readable)
- CAN message sending requires active WebSocket connection to device
- Only works when connected to the ESP32 device

## Performance

- **Memory efficient**: In-memory cache with periodic IndexedDB saves
- **Non-blocking**: IndexedDB operations are asynchronous
- **Batch processing**: Messages saved every 10 messages
- **Compact code**: ~700 lines total (can_analyzer.js + dev_tools.js)

## Future Enhancements

Potential additions (not yet implemented):

- Visual byte pattern analyzer (heatmap of changing bytes)
- Automatic pattern recognition (counter detection, scaled values, temperatures)
- Protocol documentation generator (auto-generate DETAILS.md)
- Message diff viewer (compare two snapshots)
- CAN message replay from captured sessions
- Fuzzy search for similar messages
- Correlation matrix (which messages appear together)
- Machine learning for pattern detection (TensorFlow.js)

## Browser Compatibility

Tested and working:
- Chrome/Edge 90+
- Firefox 88+
- Safari 14+

Requirements:
- IndexedDB support
- WebSocket support
- ES6+ JavaScript

## Troubleshooting

**Analyzer not working:**
- Check browser console for errors
- Verify `can_analyzer.js` loaded successfully
- Check IndexedDB is enabled (not in private/incognito mode)

**Message sending fails:**
- Verify WebSocket is connected (check status bar)
- Check ESP32 device is powered and CAN bus is initialized
- Verify CAN ID format (0x000-0x7FF for standard frames)
- Check data length matches DLC

**Data not persisting:**
- IndexedDB may be disabled in private browsing
- Browser may have cleared storage (check quota)
- Check for JavaScript errors in console

**Export/Import issues:**
- Verify JSON file is valid (not corrupted)
- Check file size isn't too large (browser limits)
- Try clearing existing data before importing

## Tips & Best Practices

1. **Start with export**: Export data early and often while learning a new protocol
2. **Use annotations**: Add notes to message IDs as you discover their purpose
3. **Check stats regularly**: Use the Stats button to identify periodic vs. event-driven messages
4. **Test sending carefully**: Always start with known-safe messages when testing CAN send
5. **Clear data**: Clear IndexedDB when switching to a different battery/protocol

## Support

For issues or questions:
- Check browser console for detailed error messages
- Review ESP32 serial output for CAN bus errors
- Verify device is connected and CAN interface is working
- Check CLAUDE.md for overall project documentation
