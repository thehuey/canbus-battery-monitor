# ESP32 Flash Partition Comparison

## Overview

The ESP32 has 4MB of flash memory. Different partition schemes divide this space differently to balance application size, OTA capabilities, and file system storage.

## Visual Layout Comparison

```
┌─────────────────────────────────────────────────────────────────────┐
│ ESP32 4MB Flash Memory (0x000000 - 0x400000)                       │
└─────────────────────────────────────────────────────────────────────┘

=== DEFAULT.CSV (Before) ===
┌──────────────┬─────┬─────────┬──────────────┬──────────────┬──────────────┬─────────┐
│ Bootloader   │ NVS │ OTAData │    APP0      │    APP1      │   SPIFFS     │CoreDump │
│   (hidden)   │ 20K │   8K    │   1.25 MB    │   1.25 MB    │   1.38 MB    │  64K    │
│              │     │         │  (running)   │  (backup)    │              │         │
└──────────────┴─────┴─────────┴──────────────┴──────────────┴──────────────┴─────────┘
0x1000       0x9000 0xE000   0x10000       0x150000       0x290000       0x3F0000 0x400000

Purpose: Supports OTA firmware updates with A/B app slots


=== HUGE_APP.CSV (Now) ===
┌──────────────┬─────┬─────────┬────────────────────────────────┬──────────┬─────────┐
│ Bootloader   │ NVS │ OTAData │            APP0                │  SPIFFS  │CoreDump │
│   (hidden)   │ 20K │   8K    │           3.00 MB              │ 896 KB   │  64K    │
│              │     │         │        (single slot)           │          │         │
└──────────────┴─────┴─────────┴────────────────────────────────┴──────────┴─────────┘
0x1000       0x9000 0xE000   0x10000                          0x310000  0x3F0000 0x400000

Purpose: Maximum application size, no OTA support
```

## Partition Details

### Common Partitions (Same in Both)

| Name      | Size  | Purpose                                           |
|-----------|-------|---------------------------------------------------|
| nvs       | 20 KB | Non-Volatile Storage - WiFi credentials, settings |
| otadata   | 8 KB  | OTA metadata - which app partition to boot        |
| coredump  | 64 KB | Core dump storage for debugging crashes          |

### Application Partitions (APP)

**DEFAULT.CSV (Before):**
- **app0**: 1.25 MB - Primary application slot
- **app1**: 1.25 MB - Secondary application slot (for OTA updates)
- **Total available**: 1.25 MB per app
- **OTA Support**: ✅ Yes - Can upload new firmware without serial cable

**HUGE_APP.CSV (Now):**
- **app0**: 3.00 MB - Single application slot
- **app1**: ❌ None
- **Total available**: 3.00 MB
- **OTA Support**: ❌ No - Must use serial cable for updates

**Firmware Size:**
- Our firmware: 1,317,701 bytes (1.26 MB)
- Would NOT fit in default.csv (1.25 MB limit)
- Fits comfortably in huge_app.csv (3.00 MB, using 41.9%)

### SPIFFS Partitions (File System)

**DEFAULT.CSV (Before):**
- Size: 1.38 MB
- Enough for: Lots of protocol files, logs, and web assets

**HUGE_APP.CSV (Now):**
- Size: 896 KB (0.88 MB)
- Current usage: ~15 KB (web files + protocols)
- Still plenty of space for:
  - Custom protocol JSON files
  - CAN log storage
  - Future additions

**What's stored in SPIFFS:**
```
/web/index.html      (7.5 KB)   - Dashboard UI
/web/app.js         (15.2 KB)   - Frontend JavaScript
/web/style.css       (8.5 KB)   - CSS styling
/protocols/*.json     (~2 KB)   - Protocol definitions
/canlogs/*.csv      (varies)    - CAN message logs
```

## Trade-offs

### DEFAULT.CSV
**Pros:**
- ✅ OTA firmware updates over WiFi
- ✅ More SPIFFS space (1.38 MB)
- ✅ Safer updates (can rollback if new firmware fails)

**Cons:**
- ❌ Limited app size (1.25 MB)
- ❌ Our firmware doesn't fit (1.26 MB)

### HUGE_APP.CSV
**Pros:**
- ✅ Large app size (3 MB) - room for features
- ✅ Simpler partition layout
- ✅ Still adequate SPIFFS space (896 KB)

**Cons:**
- ❌ No OTA updates (must use USB cable)
- ❌ Less SPIFFS space
- ❌ No rollback safety net

## Why We Changed

Our firmware (1.26 MB) exceeded the 1.25 MB app0 limit in default.csv:
```
Error: The program size (1347481 bytes) is greater than maximum allowed (1310720 bytes)
```

After code optimizations reduced it to 1.26 MB, we still needed headroom for:
- Future features
- Re-enabling TLS (adds ~2 KB)
- Re-enabling Generic BMS protocol (adds ~1-2 KB)
- Debug builds (larger than release)

## How OTA Works (default.csv only)

When OTA is enabled with two app partitions:

1. **First boot**: Firmware is in app0
2. **OTA update**: New firmware downloads to app1
3. **Verification**: Check if app1 is valid
4. **Boot flag**: Set otadata to boot from app1
5. **Reboot**: System boots from app1
6. **Next update**: Downloads to app0 (flip-flop)

This A/B pattern provides:
- Safe updates (can revert to old firmware)
- No downtime (update while running)
- Recovery from bad firmware

**With huge_app.csv, you lose this capability.**

## Alternative Partition Schemes

If you need different trade-offs, other schemes are available:

```bash
ls /home/node/.platformio/packages/framework-arduinoespressif32/tools/partitions/
```

**Popular options:**
- `default.csv` - 1.25MB app, OTA, 1.38MB SPIFFS (4MB flash)
- `huge_app.csv` - 3MB app, no OTA, 896KB SPIFFS (4MB flash)
- `min_spiffs.csv` - 1.9MB app, OTA, 192KB SPIFFS (4MB flash)
- `no_ota.csv` - 2MB app, no OTA, 2MB SPIFFS (4MB flash)
- `default_ffat_8MB.csv` - For boards with 8MB flash

## Recommendation

**For development (current):**
✅ Use `huge_app.csv` - You need the space, and USB uploads are fine during development

**For production (future):**
- If OTA is important: Optimize code further to fit in 1.25 MB
- If app size is critical: Stay with `huge_app.csv`
- Consider boards with 8MB flash for best of both worlds

## Changing Partition Schemes

To change schemes, edit `platformio.ini`:

```ini
board_build.partitions = default.csv        # or huge_app.csv, min_spiffs.csv, etc.
```

**IMPORTANT**: When you change partitions, you MUST:
1. Flash the new firmware: `pio run --target upload`
2. Upload filesystem: `pio run --target uploadfs`

The partition change erases and relocates SPIFFS, so your web files will be gone until you re-upload them.
