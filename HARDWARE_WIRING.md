# Hardware Wiring Guide

This document describes how to wire the eBike Battery CANBUS Monitor hardware.

## Bill of Materials

| Component | Quantity | Description | Notes |
|-----------|----------|-------------|-------|
| ESP32 NodeMCU-32S | 1 | Main microcontroller | Built-in WiFi, 3.3V logic |
| TJA1050 CAN Transceiver | 1 | CAN bus interface | Requires 5V supply |
| ACS712-30A | 1-5 | Current sensors | One per battery module |
| 10kΩ Resistor | 6+ | Voltage dividers | 1/4W, 1% tolerance preferred |
| 20kΩ Resistor | 6+ | Voltage dividers | 1/4W, 1% tolerance preferred |
| 120Ω Resistor | 1 | CAN bus termination | Only if end of bus |
| 5V Power Supply | 1 | Powers TJA1050 and ACS712 | 1A minimum |
| 3.3V Power Supply | 1 | Powers ESP32 | Or use ESP32 onboard regulator |

## Pin Connections Summary

### ESP32 Pin Mapping

```
                    ESP32 NodeMCU-32S
                    ┌─────────────────┐
                    │                 │
    CAN TX ───────▶ │ GPIO 5      3V3 │ ◀─── 3.3V Power
    CAN RX ◀─────── │ GPIO 4      GND │ ◀─── Ground
                    │                 │
   Current 1 ─────▶ │ GPIO 34    VP  │ ◀─── Current 5 (GPIO 36)
   Current 2 ─────▶ │ GPIO 35    VN  │ ◀─── Voltage 1 (GPIO 39)
   Current 3 ─────▶ │ GPIO 32         │
   Current 4 ─────▶ │ GPIO 33         │
                    │                 │
   Status LED ────▶ │ GPIO 2          │
                    │                 │
                    └─────────────────┘
```

## Detailed Wiring

### 1. CAN Bus Interface (TJA1050)

The TJA1050 connects the ESP32 to the eBike battery's CAN bus.

```
    eBike Battery                 TJA1050                      ESP32
    ┌───────────┐             ┌───────────────┐           ┌───────────┐
    │           │             │               │           │           │
    │     CANH  │─────────────│ CANH     TXD │───────────│ GPIO 5    │
    │           │             │               │           │           │
    │     CANL  │─────────────│ CANL     RXD │───┬───────│ GPIO 4    │
    │           │             │               │   │       │           │
    └───────────┘             │   VCC     GND │   │       │           │
                              └───────────────┘   │       │           │
                                    │       │     │       │           │
                                   5V     GND   DIVIDER   └───────────┘
```

**TJA1050 RX Voltage Divider** (5V → 3.3V):

The TJA1050 outputs 5V logic on RXD, but ESP32 GPIO is 3.3V tolerant. Use a voltage divider:

```
TJA1050 RXD ───── 10kΩ ────┬──── ESP32 GPIO 4
                           │
                          20kΩ
                           │
                          GND

Output voltage = 5V × (20kΩ / (10kΩ + 20kΩ)) = 3.33V
```

**CAN Bus Termination:**
- If this device is at the end of the CAN bus, add a 120Ω resistor between CANH and CANL
- If in the middle of the bus, no termination needed

### 2. Current Sensors (ACS712-30A)

Each battery module needs one ACS712 current sensor. The sensors are installed in-line with the battery's positive lead.

```
Battery Module ──── + ────┬──── ACS712 IP+ ────┐
                          │                     │
                          └──── ACS712 IP- ────┘──── Load (eBike Controller)
```

**ACS712 Connections:**

| ACS712 Pin | Connection |
|------------|------------|
| VCC | 5V |
| GND | Ground |
| OUT | Voltage Divider → ESP32 ADC |
| IP+ | Battery Positive |
| IP- | To Load |

**ACS712 Output Voltage Divider** (5V range → 3.3V safe):

The ACS712 outputs 2.5V at 0A, swinging ±1.5V for the ACS712-30A. Max output is ~4V at 30A.

```
ACS712 OUT ───── 10kΩ ────┬──── ESP32 ADC Pin
                          │
                         20kΩ
                          │
                         GND

Output scaling = (Vin × 20k) / (10k + 20k) = Vin × 0.667
4V max input → 2.67V max output (safe for ESP32)
```

**Current Sensor to ESP32 Pin Mapping:**

| Battery | ACS712 | ESP32 GPIO | ADC Channel |
|---------|--------|------------|-------------|
| 1 | OUT (via divider) | GPIO 34 | ADC1_CH6 |
| 2 | OUT (via divider) | GPIO 35 | ADC1_CH7 |
| 3 | OUT (via divider) | GPIO 32 | ADC1_CH4 |
| 4 | OUT (via divider) | GPIO 33 | ADC1_CH5 |
| 5 | OUT (via divider) | GPIO 36 (VP) | ADC1_CH0 |

### 3. Voltage Sensing

For measuring battery voltage, use a voltage divider to scale high voltage (e.g., 48V-72V) to ESP32-safe levels (0-3.3V).

**Voltage Divider Calculator:**

For a 72V max battery with a 20:1 ratio:
```
Battery + ───── 190kΩ ────┬──── ESP32 GPIO 39 (VN)
                          │
                         10kΩ
                          │
Battery - ───────────────GND

Divider ratio = (190k + 10k) / 10k = 20
72V input → 3.6V output (slightly over 3.3V - use 220kΩ for safety margin)
```

**Recommended Values for 72V Max:**
- R1 (top): 220kΩ → gives ratio of 23:1 → 72V → 3.13V (safe)
- R2 (bottom): 10kΩ

**Voltage Sensor Pin:**

| Battery | ESP32 GPIO | ADC Channel | Notes |
|---------|------------|-------------|-------|
| 1 | GPIO 39 (VN) | ADC1_CH3 | Primary voltage sense |
| Shared | GPIO 25 | ADC2_CH8 | For multiplexer (optional) |

### 4. Power Supply

```
5V Supply ─────┬───── TJA1050 VCC
               │
               ├───── ACS712 #1 VCC
               ├───── ACS712 #2 VCC
               ├───── ACS712 #3 VCC
               ├───── ACS712 #4 VCC
               └───── ACS712 #5 VCC

ESP32 ──────── Use USB power or external 5V to VIN pin
               (onboard regulator provides 3.3V)
```

### 5. Status LED

The ESP32 NodeMCU-32S has a built-in LED on GPIO 2.

```
GPIO 2 ──────── Built-in LED (already connected)
```

## Complete Wiring Diagram

```
                                    ┌─────────────────────┐
    ┌──────────────┐               │    eBike Battery    │
    │   5V Supply  │               │    CAN Bus          │
    └──────┬───────┘               └──────┬───┬──────────┘
           │                              │   │
           │    ┌─────────────────────────│───│────────────────────────┐
           │    │         TJA1050         │   │                        │
           │    │  ┌─────────────────┐    │   │                        │
           │    │  │  VCC   CANH     │◀───┘   │                        │
           │    │  │                 │        │                        │
           ├────┼──│  GND   CANL     │◀───────┘                        │
           │    │  │                 │                                 │
           │    │  │  TXD   RXD      │                                 │
           │    │  └───┬───────┬─────┘                                 │
           │    │      │       │                                       │
           │    │      │    ┌──┴──┐                                    │
           │    │      │    │10kΩ │                                    │
           │    │      │    └──┬──┘                                    │
           │    │      │       ├────────────────────────────┐          │
           │    │      │    ┌──┴──┐                         │          │
           │    │      │    │20kΩ │                         │          │
           │    │      │    └──┬──┘                         │          │
           │    │      │       │                            │          │
           │    │     GND     GND                           │          │
           │    │                                           │          │
           │    │  ┌─────────────────────────────────────┐  │          │
           │    │  │           ESP32 NodeMCU-32S         │  │          │
           │    │  │                                     │  │          │
           │    │  │  GPIO 5 (TX) ◀──────────────────────│──┘          │
           │    │  │  GPIO 4 (RX) ◀──────────────────────┘             │
           │    │  │                                     │             │
           │    │  │  GPIO 34 ◀── Divider ◀── ACS712 #1 OUT ◀──┐       │
           │    │  │  GPIO 35 ◀── Divider ◀── ACS712 #2 OUT ◀──┤       │
           │    │  │  GPIO 32 ◀── Divider ◀── ACS712 #3 OUT ◀──┤       │
           │    │  │  GPIO 33 ◀── Divider ◀── ACS712 #4 OUT ◀──┤       │
           │    │  │  GPIO 36 ◀── Divider ◀── ACS712 #5 OUT ◀──┤       │
           │    │  │                                     │     │       │
           │    │  │  GPIO 39 ◀── Divider ◀── Battery + ─│─────│───────┤
           │    │  │                                     │     │       │
           │    │  │  GPIO 2  ──── Status LED (built-in) │     │       │
           │    │  │                                     │     │       │
           │    │  │  3V3, GND                           │     │       │
           │    │  └─────────────────────────────────────┘     │       │
           │    │                                              │       │
           │    │  ┌───────────────────────────────────────────┼───────┤
           │    │  │              ACS712 Sensors               │       │
           │    │  │  ┌──────┐  ┌──────┐  ┌──────┐            │       │
           └────┼──┼─▶│ VCC  │  │ VCC  │  │ VCC  │  ... (x5)  │       │
                │  │  │ #1   │  │ #2   │  │ #3   │            │       │
               GND─┼─▶│ GND  │  │ GND  │  │ GND  │            │       │
                   │  │      │  │      │  │      │            │       │
                   │  │ IP+  │◀─│ IP+  │◀─│ IP+  │◀───────────┘       │
                   │  │ IP-  │─▶│ IP-  │─▶│ IP-  │─────▶ To Load      │
                   │  │      │  │      │  │      │                    │
                   │  │ OUT  │──│ OUT  │──│ OUT  │────────────────────┘
                   │  └──────┘  └──────┘  └──────┘     (via dividers)
                   │
                   └───────────────────────────────────────────────────┘
```

## Testing Connections

### Before Powering On

1. **Check all connections** for shorts using a multimeter in continuity mode
2. **Verify voltage dividers** output correct ratios with a test voltage
3. **Ensure CAN bus polarity** - CANH to CANH, CANL to CANL

### Initial Power-Up Sequence

1. Power the ESP32 via USB first
2. Check serial output at 115200 baud
3. Connect 5V supply to TJA1050 and ACS712 sensors
4. Verify WiFi AP "eBikeMonitor-XXXX" appears
5. Connect to AP and access 192.168.4.1

### CAN Bus Testing

Without the battery connected:
- CAN bus should show no traffic
- No error messages in serial output
- Web interface should show "No CAN messages"

With battery connected:
- CAN messages should appear in serial output
- Web interface shows live CAN traffic
- Use filtering to identify message IDs

### Current Sensor Calibration

1. With no current flowing, note the ADC reading
2. Use web interface `/api/calibrate/:id` to set zero point
3. Compare readings against a known ammeter

## Safety Warnings

1. **High Voltage Hazard**: eBike batteries are 36V-72V DC - potentially lethal
2. **Disconnect battery** before making any wiring changes
3. **Use appropriate fusing** on all battery connections
4. **Ensure proper insulation** on all high-voltage connections
5. **Never work on live systems**
6. **Use galvanically isolated** current sensors (ACS712 provides this)
7. **Route CAN wires** away from high-current paths

## Troubleshooting

| Issue | Possible Cause | Solution |
|-------|----------------|----------|
| No CAN messages | Wrong wiring | Check CANH/CANL connections |
| CAN errors | Missing termination | Add 120Ω if at bus end |
| Current reads 0 | Divider issue | Check voltage divider circuit |
| Current drifts | No calibration | Run zero calibration |
| Voltage too high | Wrong divider ratio | Increase R1 value |
| WiFi not appearing | ESP32 not powered | Check USB/power supply |
| Sensor NaN | ADC pin misconfigured | Verify GPIO assignments |
