# Web UI Updates - CAN Message Counter & API Links

## Summary

The mobile-friendly web interface has been enhanced with two new features:
1. **CAN Bus Message Counter** - Real-time display of CAN messages received
2. **API Endpoint Links** - Quick access to all REST API endpoints

## Changes Made

### 1. CAN Message Counter

#### Status Bar Addition
A new status item has been added to the main status bar showing the number of CAN bus messages received:

```
WiFi: MyNetwork | IP: 192.168.1.100 | Uptime: 5h 23m | CAN Msgs: 12,456
```

The counter updates in real-time via WebSocket and formats large numbers with commas for readability.

#### Settings Modal Addition
The System Information section now includes two additional fields:
- **CAN Messages**: Total messages received since boot
- **Dropped**: Number of messages dropped due to buffer overflow

### 2. API Endpoints Section

A new "API Endpoints" section has been added to the Settings modal, providing clickable links to all available REST API endpoints:

**Available Endpoints:**
- `GET /api/status` - Complete system and battery status
- `GET /api/batteries` - All battery data
- `GET /api/config` - Current configuration
- `GET /api/canlog` - Recent CAN messages (JSON)
- `GET /api/canlog/download` - Download CAN log as CSV
- `VIEW /logs` - Live log viewer page

Each link opens in a new tab and shows the raw JSON response, making it easy to:
- Test API endpoints
- Debug data issues
- Integrate with external systems
- Understand the data structure

## Technical Implementation

### Backend Changes

**`src/network/web_server.cpp`**
- Updated `buildSystemJSON()` to include CAN statistics:
  ```cpp
  obj["can_message_count"] = can_logger_->getMessageCount();
  obj["can_dropped_count"] = can_logger_->getDroppedCount();
  ```

### Frontend Changes

**`data/web/index.html`**
- Added CAN message counter to status bar
- Added CAN statistics to System Information section
- Added API Endpoints section with styled links

**`data/web/app.js`**
- Updated `updateSystemStatus()` to display CAN message count
- Added `formatNumber()` helper for comma-separated numbers
- Updates CAN stats in both status bar and settings modal

**`data/web/style.css`**
- Added `.api-links` styles for the API endpoint section
- Added `.api-method` badge styling (colored GET/POST badges)
- Added `.api-path` monospace font for endpoint paths
- Added hover effects with slide animation

## Visual Design

### API Links Styling
Each API link is displayed as a card with:
- **Method Badge**: Colored label (GET/POST/VIEW) in the primary theme color
- **Endpoint Path**: Monospace font for clarity
- **Hover Effect**: Slide to the right with border color change
- **Mobile-Friendly**: Large touch targets, responsive layout

### Status Bar
The status bar now shows 4 items on larger screens:
- WiFi status (network name or "AP Mode")
- IP address
- Uptime (formatted as days/hours/minutes)
- CAN message count (formatted with commas)

On smaller screens, the items wrap naturally for mobile viewing.

## Usage

### Viewing CAN Message Count

1. **Main Dashboard**: Check the status bar at the top
2. **Settings**: Click the gear icon → System Information section

The counter increments in real-time as messages are received from the battery modules.

### Using API Links

1. Click the **gear icon** to open Settings
2. Scroll to **API Endpoints** section
3. Click any endpoint link to view its response in a new tab

**Example Response** (`/api/status`):
```json
{
  "system": {
    "uptime_ms": 1234567,
    "free_heap": 89916,
    "can_message_count": 12456,
    "can_dropped_count": 0,
    "wifi_connected": true,
    "wifi_ip": "192.168.1.100"
  },
  "batteries": {
    "batteries": [
      {
        "id": 0,
        "name": "Battery 1",
        "voltage": 52.4,
        "current": 3.2,
        "power": 167.68,
        "soc": 85
      }
    ],
    "total_power": 167.68
  }
}
```

## Integration Examples

### Using the API with curl

```bash
# Get system status
curl http://192.168.1.100/api/status

# Get CAN messages with filter
curl http://192.168.1.100/api/canlog?filter=0x201

# Download CAN log
curl http://192.168.1.100/api/canlog/download -o canlog.csv
```

### Using with MQTT

The device also publishes data via MQTT (if configured):
- Topic: `ebike/battery/0/status`
- Topic: `ebike/battery/all/status`
- Topic: `ebike/system/status`

### Home Assistant Integration

```yaml
sensor:
  - platform: rest
    name: "eBike CAN Messages"
    resource: "http://192.168.1.100/api/status"
    value_template: "{{ value_json.system.can_message_count }}"
    unit_of_measurement: "msgs"

  - platform: rest
    name: "eBike Battery Power"
    resource: "http://192.168.1.100/api/batteries"
    value_template: "{{ value_json.total_power }}"
    unit_of_measurement: "W"
```

## Memory Usage

The updates added minimal overhead:
- **Flash**: Still at 41.9% (1,317,937 / 3,145,728 bytes)
- **RAM**: Unchanged at 27.4% (89,916 / 327,680 bytes)
- **SPIFFS**: 16.8 KB (web files compressed to 16,789 bytes)

## Browser Compatibility

Tested and working on:
- ✅ Mobile Safari (iOS)
- ✅ Chrome Mobile (Android)
- ✅ Desktop Chrome/Firefox/Safari
- ✅ Edge

## Future Enhancements

Potential additions:
- Real-time CAN message viewer with live updates
- Message rate graph (messages/second)
- Filter CAN messages by ID in the UI
- Export filtered messages to CSV from UI
- API endpoint search/filter
- Copy API response to clipboard button
