# Web UI: CAN Logging Settings

## Overview

Added user-friendly toggle switches in the web settings interface to control CAN logging and MQTT publishing features directly from the browser.

## Features

### New Settings Section: "CAN Logging & Publishing"

Located in the Settings modal between "MQTT Configuration" and "System Actions", this section provides two checkbox toggles:

#### 1. Local CAN Logging
- **Label**: "Local CAN Logging"
- **Description**: "Store CAN messages to SPIFFS (/api/canlog)"
- **Backend Setting**: `can_log_enabled`
- **Default**: Checked (enabled)

**When Enabled**:
- CAN messages are stored locally on SPIFFS
- Accessible via `/api/canlog` endpoint
- Can be downloaded as CSV via `/api/canlog/download`

**When Disabled**:
- No CAN messages stored locally
- Saves SPIFFS storage space
- `/api/canlog` returns empty

#### 2. MQTT CAN Publishing
- **Label**: "MQTT CAN Publishing"
- **Description**: "Publish CAN messages to MQTT topic: <prefix>/canmsg"
- **Backend Setting**: `mqtt_canmsg_enabled`
- **Default**: Unchecked (disabled)

**When Enabled**:
- Each CAN message published to MQTT topic in real-time
- Topic format: `<mqtt_topic_prefix>/canmsg`
- JSON format with id, dlc, data, timestamp, extended, rtr

**When Disabled**:
- No MQTT publishing of CAN messages
- Reduces network traffic

### Save Button

- **Text**: "Save CAN Settings"
- **Action**: Sends POST request to `/api/config` with updated settings
- **Feedback**: Toast notification on success/failure
- **Effect**: Settings immediately applied (no reboot required)
- **Persistence**: Settings saved to NVS

## User Interface

### Visual Design

```
┌─────────────────────────────────────────────────────────┐
│ CAN Logging & Publishing                                │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  ┌──────────────────────────────────────────────────┐  │
│  │ ☑ Local CAN Logging                             │  │
│  │   Store CAN messages to SPIFFS (/api/canlog)    │  │
│  └──────────────────────────────────────────────────┘  │
│                                                          │
│  ┌──────────────────────────────────────────────────┐  │
│  │ ☐ MQTT CAN Publishing                           │  │
│  │   Publish CAN messages to MQTT topic:           │  │
│  │   <prefix>/canmsg                               │  │
│  └──────────────────────────────────────────────────┘  │
│                                                          │
│  [ Save CAN Settings ]                                  │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

### CSS Styling

- **Container**: Light border with rounded corners, card background
- **Checkboxes**: 1.2x scale for easy mobile interaction
- **Layout**: Flexbox with checkbox on left, text on right
- **Typography**: Bold label, smaller gray help text below
- **Spacing**: Consistent padding and margins for readability
- **Responsive**: Mobile-friendly with touch-optimized controls

## Implementation Details

### HTML Changes (`data/web/index.html`)

Added new `<div class="settings-section">` with:
- Form ID: `canLoggingForm`
- Two checkbox inputs with IDs: `canLogEnabled`, `mqttCanmsgEnabled`
- Structured labels with checkbox and descriptive text
- Submit button for saving

```html
<div class="settings-section">
  <h3>CAN Logging & Publishing</h3>
  <form id="canLoggingForm">
    <div class="form-group checkbox-group">
      <label class="checkbox-label">
        <input type="checkbox" id="canLogEnabled" name="can_log_enabled" />
        <span class="checkbox-text">
          <strong>Local CAN Logging</strong>
          <small class="form-help">Store CAN messages to SPIFFS (/api/canlog)</small>
        </span>
      </label>
    </div>
    <!-- ... -->
  </form>
</div>
```

### JavaScript Changes (`data/web/app.js`)

#### 1. Event Listener Registration
```javascript
setupEventListeners() {
    // ...
    document.getElementById('canLoggingForm').addEventListener('submit', (e) => {
        e.preventDefault();
        this.saveCANLoggingConfig();
    });
}
```

#### 2. Config Population
```javascript
populateConfigForm() {
    // ...
    document.getElementById('canLogEnabled').checked =
        this.config.can_log_enabled !== false;
    document.getElementById('mqttCanmsgEnabled').checked =
        this.config.mqtt_canmsg_enabled === true;
}
```

**Logic**:
- `can_log_enabled`: Defaults to `true` if undefined (backwards compatibility)
- `mqtt_canmsg_enabled`: Defaults to `false` if undefined (opt-in feature)

#### 3. Save Handler
```javascript
async saveCANLoggingConfig() {
    const config = {
        can_log_enabled: document.getElementById('canLogEnabled').checked,
        mqtt_canmsg_enabled: document.getElementById('mqttCanmsgEnabled').checked
    };

    const response = await fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(config)
    });

    const result = await response.json();

    if (response.ok && result.success) {
        this.showToast('CAN logging settings saved!', 'success');
        this.loadConfig();  // Refresh form
    } else {
        this.showToast(result.message || 'Failed to save', 'error');
    }
}
```

### CSS Changes (`data/web/style.css`)

Added new classes:

```css
.checkbox-group {
    padding: 0.75rem;
    background: var(--bg-card);
    border: 1px solid var(--border-color);
    border-radius: 8px;
}

.checkbox-label {
    display: flex;
    align-items: flex-start;
    gap: 0.75rem;
    cursor: pointer;
}

.checkbox-label input[type="checkbox"] {
    transform: scale(1.2);  /* Larger for mobile */
    cursor: pointer;
}

.checkbox-text {
    display: flex;
    flex-direction: column;
}
```

## User Workflow

### Scenario 1: Enable MQTT Publishing

1. User opens web dashboard
2. Clicks Settings (gear icon)
3. Scrolls to "CAN Logging & Publishing" section
4. Checks "MQTT CAN Publishing" checkbox
5. Clicks "Save CAN Settings"
6. Sees success toast: "CAN logging settings saved!"
7. CAN messages immediately start publishing to MQTT

### Scenario 2: Disable Local Logging (Save Storage)

1. User opens Settings
2. Unchecks "Local CAN Logging"
3. Clicks "Save CAN Settings"
4. Local logging stops immediately
5. Existing logs remain accessible until manually cleared
6. New messages not stored to SPIFFS

### Scenario 3: Check Current Status

1. User opens Settings
2. Views checkmarks on both toggles
3. Can see at a glance:
   - ✓ Local logging is ON
   - ✗ MQTT publishing is OFF

## API Integration

### GET `/api/config`

Response includes:
```json
{
  "can_log_enabled": true,
  "mqtt_canmsg_enabled": false,
  ...
}
```

### POST `/api/config`

Request:
```json
{
  "can_log_enabled": false,
  "mqtt_canmsg_enabled": true
}
```

Response:
```json
{
  "success": true,
  "message": "Configuration saved"
}
```

## Mobile Responsiveness

- Touch-friendly checkbox size (1.2x scale)
- Adequate spacing between clickable elements
- No hover states (works on touch devices)
- Full-width form on small screens
- Easy to read on 320px+ widths

## Accessibility

- Semantic HTML with proper `<label>` associations
- Checkboxes are keyboard navigable (Tab key)
- Form submits on Enter key
- Clear visual feedback on focus
- High contrast text for readability

## Error Handling

### Network Errors
```javascript
try {
    // API call
} catch (error) {
    this.showToast('Network error - check connection', 'error');
}
```

### API Errors
```javascript
if (!response.ok) {
    this.showToast(result.message || 'Failed to save', 'error');
}
```

### Invalid State
- Config reload on successful save ensures form matches backend
- Checkboxes have safe defaults if API returns undefined values

## Testing

### Manual Testing Steps

1. **Initial Load**
   - Open Settings
   - Verify checkboxes reflect current settings
   - Default: Local logging ON, MQTT OFF

2. **Enable MQTT**
   - Check "MQTT CAN Publishing"
   - Click "Save CAN Settings"
   - Verify success toast
   - Verify checkbox remains checked after reload

3. **Disable Local Logging**
   - Uncheck "Local CAN Logging"
   - Save settings
   - Verify `/api/canlog` returns empty
   - Re-enable and verify logs resume

4. **Both Enabled**
   - Enable both checkboxes
   - Save and verify both features work simultaneously

5. **Both Disabled**
   - Disable both checkboxes
   - Save and verify neither logging nor MQTT occurs

6. **Page Refresh**
   - Change settings
   - Refresh browser
   - Open Settings
   - Verify checkboxes persist

## Future Enhancements

Potential additions:
- Status indicators showing active state (not just configured)
- Message rate display (messages/sec for local and MQTT)
- Quick action buttons ("Clear Local Log", "Test MQTT Publish")
- Advanced options (filters, rate limiting)
- Visual feedback when messages are actively flowing

## Files Modified

1. `data/web/index.html` - Added CAN Logging section with checkboxes
2. `data/web/app.js` - Added event listener, config loading, save handler
3. `data/web/style.css` - Added checkbox styling for clean UI
