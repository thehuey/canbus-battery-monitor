# CAN Protocol JSON Schema

This document describes the JSON schema for defining battery CAN protocols.

## Protocol File Structure

```json
{
  "name": "string",              // Human-readable protocol name
  "manufacturer": "string",      // Manufacturer name
  "version": "string",           // Protocol version (e.g., "1.0")
  "description": "string",       // Detailed description
  "cell_count": number,          // Number of cells in series (0 if unknown)
  "nominal_voltage": number,     // Nominal pack voltage in volts
  "capacity_ah": number,         // Pack capacity in amp-hours
  "chemistry": "string",         // Battery chemistry (e.g., "Li-ion", "LiFePO4")
  "messages": [...]              // Array of CAN message definitions
}
```

## Message Definition

```json
{
  "can_id": number,              // CAN ID as decimal (e.g., 513 for 0x201)
  "can_id_hex": "string",        // CAN ID as hex string (e.g., "0x201")
  "name": "string",              // Message name
  "description": "string",       // Message description
  "period_ms": number,           // Expected message period in milliseconds
  "fields": [...]                // Array of field definitions
}
```

## Field Definition

```json
{
  "name": "string",              // Field name (use snake_case)
  "description": "string",       // Field description
  "byte_offset": number,         // Starting byte position (0-7)
  "length": number,              // Length in bytes (1, 2, 4)
  "data_type": "string",         // Data type (see below)
  "unit": "string",              // Physical unit (e.g., "mV", "mA", "°C", "%")
  "scale": number,               // Multiplication factor
  "offset": number,              // Additive offset
  "formula": "string",           // Optional: formula description (e.g., "value / 13")
  "min_value": number,           // Optional: minimum valid value (after scaling)
  "max_value": number,           // Optional: maximum valid value (after scaling)
  "enum_values": {...},          // Optional: enumeration mapping
  "note": "string"               // Optional: additional notes
}
```

## Supported Data Types

| Type        | Description                           | Bytes |
|-------------|---------------------------------------|-------|
| `uint8`     | Unsigned 8-bit integer                | 1     |
| `int8`      | Signed 8-bit integer                  | 1     |
| `uint16_le` | Unsigned 16-bit integer (little-endian) | 2   |
| `uint16_be` | Unsigned 16-bit integer (big-endian)    | 2   |
| `int16_le`  | Signed 16-bit integer (little-endian)   | 2   |
| `int16_be`  | Signed 16-bit integer (big-endian)      | 2   |
| `uint32_le` | Unsigned 32-bit integer (little-endian) | 4   |
| `uint32_be` | Unsigned 32-bit integer (big-endian)    | 4   |
| `int32_le`  | Signed 32-bit integer (little-endian)   | 4   |
| `int32_be`  | Signed 32-bit integer (big-endian)      | 4   |
| `float_le`  | IEEE 754 float (little-endian)          | 4   |
| `float_be`  | IEEE 754 float (big-endian)             | 4   |

## Value Calculation

The final value is calculated as:

```
physical_value = (raw_value * scale) + offset
```

### Examples

1. **Temperature with offset:**
   - Raw value: 65
   - Scale: 1.0
   - Offset: -40.0
   - Result: (65 × 1.0) + (-40.0) = 25°C

2. **Voltage with scale:**
   - Raw value: 52400
   - Scale: 0.001
   - Offset: 0.0
   - Result: (52400 × 0.001) + 0 = 52.4V

3. **Current with both:**
   - Raw value: 34000
   - Scale: 0.1
   - Offset: -3200.0
   - Result: (34000 × 0.1) + (-3200.0) = 200mA

4. **Average cell voltage:**
   - Raw value: 53054 (total pack voltage in mV)
   - Scale: 0.07692307692 (1/13)
   - Offset: 0.0
   - Result: (53054 × 0.07692307692) = 4081.8mV per cell

## Enum Values

For state machines or discrete values, use `enum_values`:

```json
{
  "name": "state",
  "data_type": "uint8",
  "enum_values": {
    "0": "idle",
    "1": "charging",
    "2": "discharging",
    "16": "charge_complete",
    "255": "fault"
  }
}
```

The keys are the raw decimal values (as strings), and the values are the symbolic names.

## Validation Rules

1. `byte_offset` must be 0-7
2. `byte_offset + length` must not exceed 8 (CAN frame size)
3. `length` must match the data type size
4. `scale` must not be zero
5. If `enum_values` is present, `unit` should typically be null
6. `can_id` must be 0-2047 for standard frames (11-bit ID)

## Best Practices

1. Use descriptive field names (e.g., `pack_voltage_mv`, not `pv`)
2. Always specify units for physical quantities
3. Document unknown fields as `unknown_byte_N` or `unknown_word_N`
4. Use `note` field to indicate uncertainty or validation needed
5. Set reasonable `min_value` and `max_value` for range checking
6. Keep protocol versions for backwards compatibility
