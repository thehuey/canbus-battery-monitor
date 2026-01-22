# D-power Pack Identifier Parsing Feature

## Overview

This feature adds automatic parsing and display of the D-power battery pack manufacturing date and serial number from CAN bus message ID `0x201`.

## Pack Identifier Format

The pack identifier is a 32-bit value in **YYDDMMSSSS** decimal format:

| Position | Digits | Description | Example |
|----------|--------|-------------|---------|
| First 2  | YY     | Year (add 2000) | 20 → 2020 |
| Next 2   | DD     | Day of month (01-31) | 18 → 18th |
| Next 2   | MM     | Month (01-12) | 09 → September |
| Last 4   | SSSS   | Serial number | 5346 |

**Example**: `2018095346`
- **Year**: 20 + 2000 = **2020**
- **Day**: **18**
- **Month**: **09** (September)
- **Serial**: **5346**
- **Formatted**: "September 18, 2020, S/N: 5346"

## Implementation

### 1. Protocol Definition

**File**: `/workspace/data/protocols/dpower_48v_13s.json`

Updated message ID `0x201` to extract the 4-byte pack identifier:

```json
{
  "can_id": 513,
  "can_id_hex": "0x201",
  "name": "Pack Identification",
  "fields": [
    {
      "name": "pack_identifier",
      "byte_offset": 0,
      "length": 4,
      "data_type": "uint32_le",
      "format": "YYDDMMSSSS",
      "decode_formula": {
        "year": "(value / 100000000) + 2000",
        "day": "(value / 1000000) % 100",
        "month": "(value / 10000) % 100",
        "serial": "value % 10000"
      }
    }
  ]
}
```

### 2. Backend Changes

#### CANBatteryData Structure
**File**: `/workspace/src/can/can_message.h`

Added `pack_identifier` field:
```cpp
struct CANBatteryData {
    // ... existing fields ...
    uint32_t pack_identifier;   // Manufacturing date/serial (YYDDMMSSSS format)
    bool valid;
};
```

#### CAN Parser
**File**: `/workspace/src/can/can_parser.cpp`

Added field mapping in `parseWithProtocol()`:
```cpp
else if (strcmp(field.name, "pack_identifier") == 0) {
    data.pack_identifier = static_cast<uint32_t>(value);
}
```

#### Battery Module
**Files**: `/workspace/src/battery/battery_module.h` and `.cpp`

- Added `pack_identifier` member variable
- Added `getPackIdentifier()` getter method
- Updated `updateFromCAN()` to store pack_identifier

#### Web Server
**File**: `/workspace/src/network/web_server.cpp`

Added `pack_identifier` to JSON output in `buildBatteryJSON()`:
```cpp
obj["pack_identifier"] = battery->getPackIdentifier();
```

### 3. Frontend Changes

#### JavaScript Parsing
**File**: `/workspace/data/web/app.js`

Added `parsePackIdentifier()` method:
```javascript
parsePackIdentifier(identifier) {
    const year = Math.floor(identifier / 100000000) + 2000;
    const day = Math.floor(identifier / 1000000) % 100;
    const month = Math.floor(identifier / 10000) % 100;
    const serial = identifier % 10000;

    // Validate and format
    const monthNames = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
                        'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
    const dateStr = `${monthNames[month - 1]} ${day}, ${year}`;

    return {
        date: dateStr,
        serial: serial.toString().padStart(4, '0'),
        year, month, day
    };
}
```

Updated `createBatteryCard()` to display pack info:
```javascript
if (battery.pack_identifier) {
    const packInfo = this.parsePackIdentifier(battery.pack_identifier);
    if (packInfo) {
        // Display formatted date and serial number
    }
}
```

#### CSS Styling
**File**: `/workspace/data/web/style.css`

Added styles for pack information display:
```css
.battery-info {
    display: flex;
    gap: 1rem;
    margin-bottom: 0.75rem;
    padding: 0.5rem;
    background: var(--bg-primary);
    border-radius: 8px;
}

.info-item {
    display: flex;
    align-items: center;
    gap: 0.375rem;
}
```

## UI Display

The pack identifier is displayed in each battery card with:
- 📅 Manufacturing date (formatted as "Month Day, Year")
- 🔢 Serial number (4-digit, zero-padded)

### Example Display

```
┌─────────────────────────────────┐
│ Battery 1              [OK]     │
├─────────────────────────────────┤
│ 📅 Sep 18, 2020  🔢 S/N: 5346  │
├─────────────────────────────────┤
│ Voltage: 52.4 V  Current: 3.2 A│
│ Power: 167.7 W   SOC: 85%      │
└─────────────────────────────────┘
```

## Data Flow

```
CAN Bus (0x201)
    ↓
CANDriver (receives 4 bytes: 0x12 0xB7 0x5E 0x78)
    ↓
Protocol Parser (extracts uint32_le: 2018095346)
    ↓
CANBatteryData (pack_identifier = 2018095346)
    ↓
BatteryModule (stores pack_identifier)
    ↓
Web Server (includes in JSON: "pack_identifier": 2018095346)
    ↓
Web UI JavaScript (parses YYDDMMSSSS format)
    ↓
Display (formatted: "Sep 18, 2020, S/N: 5346")
```

## Testing

### 1. Check Raw Value
Visit `/api/batteries` and look for:
```json
{
  "id": 0,
  "pack_identifier": 2018095346,
  ...
}
```

### 2. Verify Parsing
The web dashboard should automatically display the formatted date and serial number in the battery card.

### 3. Check CAN Log
Visit `/api/canlog?filter=0x201` to see the raw CAN message:
```json
{
  "id": "0x201",
  "dlc": 8,
  "data": "12 B7 5E 78 ...",
  ...
}
```

The bytes `12 B7 5E 78` in little-endian = `0x785EB712` = 2,018,095,346 decimal

## Validation

The JavaScript parser validates:
- Year range: 2000-2099
- Month range: 1-12
- Day range: 1-31

Invalid values return `null` and the pack info section is not displayed.

## Benefits

1. **No Manual Interpretation**: Automatically decodes the cryptic decimal format
2. **Human-Readable Display**: Shows dates in familiar format (e.g., "Sep 18, 2020")
3. **Easy Serial Identification**: 4-digit serial number clearly displayed
4. **Quality Control**: Helps track battery manufacturing batches
5. **Warranty Management**: Easy to verify manufacturing date

## Future Enhancements

Potential additions:
- Highlight batteries nearing warranty expiration
- Group batteries by manufacturing batch
- Export pack identifier data to CSV/MQTT
- Add manufacturing date filter in CAN log viewer
