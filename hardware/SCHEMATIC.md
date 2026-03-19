# eBike Battery Monitor — Circuit Schematic

## Board Overview

A single custom PCB integrating:
- ESP32-WROOM-32E module
- TJA1050 CAN bus transceiver
- ACS712-30A current sensor (on-board, with headers for 4 additional external modules)
- TPS54360 wide-input buck converter (20–60 V → 5 V)
- AMS1117-3.3 LDO (5 V → 3.3 V)
- USB-C for programming/debugging
- Status LED, connectors, and protection circuitry

Board dimensions target: 80 mm × 55 mm (2-layer, 1.6 mm FR4)

---

## 1. Power Supply

### 1.1 Input Protection

```
DC INPUT (20–60 V)
  │
  ├── F1: Polyfuse 3A (1812 package)
  │
  ├── D1: SMBJ60A (TVS diode, 60 V standoff, SMB package)
  │     └── Clamps transients from battery spikes
  │
  ├── Q1: SI2301 P-MOSFET (reverse polarity protection)
  │     Gate → Cathode of D2 (Zener 12V) + 100kΩ to drain
  │     Source → F1 output
  │     Drain → V_IN rail
  │
  └── C1: 100 µF / 80 V electrolytic (input bulk)
      C2: 100 nF / 100 V ceramic (1206, close to TPS54360)
```

**Why P-MOSFET instead of a series diode?** A diode drops 0.5–0.7 V and wastes
power at high current. The P-FET has < 50 mΩ Rds(on), essentially zero drop.

### 1.2 Stage 1: Buck Converter — 20–60 V → 5 V

Using **TPS54360** (TI) — 4.5–60 V input, 3.5 A output, 300 kHz switching.

```
                          TPS54360 (SOIC-8 with exposed pad)
                         ┌──────────────────┐
           V_IN ────────▶│ 1 BOOT    VIN  8 │◀── V_IN
                         │                   │
           BST_CAP ─────▶│ 2 PH    EN/UVLO 7│◀── Enable (see below)
                         │                   │
           L1 output ◀──│ 3 PH     COMP   6│──── Compensation network
                         │                   │
              GND ──────▶│ 4 GND     FB   5 │◀── Feedback divider
                         │   (exposed pad)   │
                         └──────────────────┘

Boot capacitor:
  C_BST: 100 nF ceramic between BOOT (pin 1) and PH (pin 2)

Output inductor:
  L1: 15 µH, 4 A saturation, shielded (e.g., Bourns SRN8040-150M)
  Connected from PH (pin 2/3) to V_5V rail

Output capacitors:
  C3: 100 µF / 10 V electrolytic
  C4: 22 µF / 10 V ceramic (1210 X5R)
  C5: 100 nF / 10 V ceramic (0805)

Feedback divider (sets output to 5.0 V):
  R_TOP: 49.9 kΩ (1%, 0805) — V_5V to FB (pin 5)
  R_BOT: 12.4 kΩ (1%, 0805) — FB (pin 5) to GND
  V_OUT = 0.8 V × (1 + R_TOP/R_BOT) = 0.8 × (1 + 49.9/12.4) = 5.02 V

Compensation network (Type II, on COMP pin 6):
  R_COMP: 30 kΩ (0805)
  C_COMP1: 3.3 nF (0805) — in series with R_COMP, from COMP to GND
  C_COMP2: 47 pF (0402) — from COMP to GND

EN/UVLO (pin 7) — Under-voltage lockout to prevent startup below 18 V:
  R_EN1: 1 MΩ (0805) — V_IN to EN
  R_EN2: 270 kΩ (0805) — EN to GND
  Turn-on threshold: 1.18 V × (1 + 1M/270k) = ~5.55 V at EN
  Corresponding V_IN: ~20.5 V (with hysteresis from internal 3.4 µA current)

Catch diode:
  D3: SS340 (3A, 40V Schottky, SMA package) — from GND to PH
  (TPS54360 has internal high-side FET, external diode needed for buck)
```

### 1.3 Stage 2: LDO — 5 V → 3.3 V

Using **AMS1117-3.3** (SOT-223, 1A output, fixed 3.3 V).

```
         V_5V ──┬── C6: 10 µF ceramic (1206)
                │
                ▼
         ┌──────────────┐
         │   AMS1117    │
         │ IN    OUT    │──┬── V_3V3 rail
         │     GND      │  │
         └──────┬───────┘  ├── C7: 22 µF ceramic (1206)
                │          └── C8: 100 nF ceramic (0805)
               GND
```

### Power Budget

| Rail  | Load                          | Current Est. |
|-------|-------------------------------|-------------|
| 5 V   | TJA1050 (75 mA) + ACS712 (13 mA) + headroom | ~150 mA |
| 3.3 V | ESP32 WiFi active (240 mA avg, 500 mA peak) | ~350 mA |
| **Total from 5 V** | Including LDO load | **~500 mA** |

TPS54360 @ 500 mA from 60 V: well within 3.5 A limit. Efficiency ~85%.

---

## 2. ESP32 Module

Using **ESP32-WROOM-32E** (castellated or through-hole, 38-pin).

```
                    ESP32-WROOM-32E
              ┌────────────────────────┐
    V_3V3 ───▶│ 3V3                GND │──── GND
              │                        │
              │ EN ◀── R6 10kΩ to 3V3  │     (with C_EN 100nF to GND for noise)
              │        + SW1 (RST btn) │
              │                        │
              │ GPIO0 ◀── R7 10kΩ 3V3  │     (with SW2 BOOT button to GND)
              │                        │
   CAN TX ◀──│ GPIO5              IO34 │──── ACS712 Batt 1 (ADC, input only)
   CAN RX ───│ GPIO4              IO35 │──── ACS712 Batt 2 (ADC, input only)
              │                    IO32 │──── ACS712 Batt 3 (ADC)
   LED ◀─────│ GPIO2              IO33 │──── ACS712 Batt 4 (ADC)
              │                    IO36 │──── ACS712 Batt 5 (ADC, VP, input only)
   USB TX ◀──│ GPIO1 (TX0)        IO39 │──── Voltage Sense (ADC, VN, input only)
   USB RX ───│ GPIO3 (RX0)        IO25 │──── (spare / voltage mux)
              │                        │
              │ GPIO16             IO26 │──── (spare — SDA for display)
              │ GPIO17             IO27 │──── (spare — SCL for display)
              │                        │
              └────────────────────────┘

Decoupling (close to 3V3 pin):
  C9:  100 nF ceramic (0805)
  C10: 10 µF ceramic (0805)
```

### 2.1 USB-C Programming Interface

Using **CH340C** (built-in oscillator, no external crystal needed).

```
        USB-C Connector (USB 2.0 only, CC resistors for detection)
        ┌───────────────┐
   VBUS─│ A4/B4    GND  │─── GND
        │ A1/B1(GND)    │
    D+──│ A6/B6         │     CC1 ── 5.1kΩ ── GND  (identifies as UFP/device)
    D-──│ A7/B7         │     CC2 ── 5.1kΩ ── GND
        └───────────────┘
             │  │
        ESD: USBLC6-2SC6  (on D+/D- lines)
             │  │
             ▼  ▼
         ┌──────────┐
    D+ ──│ UD+   TX │──── ESP32 GPIO3 (RX0)
    D- ──│ UD-   RX │──── ESP32 GPIO1 (TX0)
   VBUS──│ VCC  DTR │──┬─ C11 100nF ── ESP32 EN (auto-reset)
    GND──│ GND  RTS │──┴─ C12 100nF ── ESP32 GPIO0 (auto-boot)
         │  CH340C  │
         └──────────┘

Note: DTR/RTS auto-reset circuit uses the standard two-transistor
      or capacitor-coupled method for one-click flash from PlatformIO.
```

### 2.2 Status LED

```
  GPIO2 ── R8 1kΩ ──▶|── GND
                    (LED, green, 0805)
```

---

## 3. CAN Bus Interface

Using **TJA1050** (SOIC-8), powered from 5 V rail.

```
                    TJA1050
              ┌─────────────────┐
   V_5V ────▶│ 3 VCC    TXD  1 │◀── ESP32 GPIO5 (CAN TX, 3.3V OK for TJA input)
              │                  │
    GND ────▶│ 2 GND    RXD  4 │──┬── RX voltage divider ── ESP32 GPIO4
              │                  │  │
   CANH ◀───│ 7 CANH    RS  8 │──── GND (slope control: high speed mode)
              │                  │
   CANL ◀───│ 6 CANL   VREF 5 │──── (leave floating or 100nF to GND)
              └─────────────────┘

Decoupling:
  C13: 100 nF ceramic (0805) close to VCC pin

RX Level Shifter (5V → 3.3V):
  TJA1050 RXD (pin 4) ── R9 10kΩ ──┬── ESP32 GPIO4
                                     │
                                    R10 20kΩ
                                     │
                                    GND

  Output voltage: 5V × 20k/(10k+20k) = 3.33V ✓

CAN Bus Connector (screw terminal, 3-pin):
  Pin 1: CANH
  Pin 2: CANL
  Pin 3: GND (shield/reference)

Termination (optional, active via jumper):
  JP1 (2-pin header with jumper cap):
    CANH ── JP1 ── R11 120Ω ── CANL

  Place jumper when this board is at end of CAN bus.

ESD Protection:
  D4: PESD2CAN (NXP) — dual TVS on CANH/CANL lines
  Place as close to screw terminal as possible.
```

---

## 4. Current Sensing — ACS712-30A

### 4.1 On-Board ACS712 (Battery 1)

The ACS712-30A is available in SOIC-8 but the high-current IP± pins
require wide traces. For the on-board sensor:

```
                     ACS712-30A (SOIC-8)
              ┌──────────────────────┐
  IP+ (in) ──│ 1 IP+     VCC    8 │◀── V_5V
  IP+ (in) ──│ 2 IP+     VIOUT  7 │──── Filter + Divider → ESP32 GPIO34
  IP- (out)──│ 3 IP-     FILTER 6 │──── C_F 1nF to GND (bandwidth filter)
  IP- (out)──│ 4 IP-     GND    5 │──── GND
              └──────────────────────┘

Decoupling:
  C14: 100 nF ceramic (0805) close to VCC (pin 8)

VIOUT → ESP32 ADC voltage divider (scales 0–5V to 0–3.3V):
  ACS712 VIOUT (pin 7) ── R12 10kΩ ──┬── ESP32 GPIO34
                                       │
                                      R13 20kΩ
                                       │
                                      GND
                                       │
                                      C15 100nF (filter cap, 0805)

IP± Trace Requirements:
  - Minimum 2.5 mm (100 mil) trace width for 30A
  - Use both copper layers (vias to stitch top and bottom)
  - Heavy copper (2 oz) recommended for IP± path
  - Screw terminal or XT30/XT60 connector for current path

Current Path Connector:
  J3 (2-pos screw terminal, 5.08 mm pitch, rated 30A):
    Pin 1: IP+ (battery positive in)
    Pin 2: IP- (load positive out)
```

### 4.2 External ACS712 Headers (Batteries 2–5)

Four 3-pin headers for connecting external ACS712 breakout modules:

```
  J4 (Battery 2):  VCC(5V) | GND | VIOUT → divider → GPIO35
  J5 (Battery 3):  VCC(5V) | GND | VIOUT → divider → GPIO32
  J6 (Battery 4):  VCC(5V) | GND | VIOUT → divider → GPIO33
  J7 (Battery 5):  VCC(5V) | GND | VIOUT → divider → GPIO36

Each VIOUT line has its own voltage divider (10kΩ / 20kΩ) and
100 nF filter capacitor, identical to Battery 1 circuit.
```

---

## 5. Voltage Sensing

### 5.1 Battery Voltage Divider

Measures pack voltage (20–60 V) scaled to 0–3.15 V for ESP32 ADC.

```
  V_BATT ── R14 190kΩ (1%, 0805) ──┬── ESP32 GPIO39 (VN)
                                     │
                                    R15 10kΩ (1%, 0805)
                                     │
                                    GND
                                     │
                                    C16 100nF (filter, 0805)

  Ratio: 10k / (190k + 10k) = 1/20
  At 60V: 60 / 20 = 3.0V  ✓ (within 3.3V ADC range)
  At 20V: 20 / 20 = 1.0V  ✓

  Resistor power dissipation at 60V:
  I = 60V / 200kΩ = 0.3 mA → P_R14 = 0.3² × 190k = 17 mW (fine for 0805)
```

---

## 6. Display Header (Optional I2C OLED/TFT)

```
  J8 (4-pin header, 2.54mm):
    Pin 1: GND
    Pin 2: V_3V3
    Pin 3: SDA → ESP32 GPIO26
    Pin 4: SCL → ESP32 GPIO27

  Pull-ups on I2C bus:
    R16: 4.7kΩ, SDA to V_3V3
    R17: 4.7kΩ, SCL to V_3V3
```

---

## 7. Complete Schematic — Net Summary

### Power Nets
| Net Name | Voltage | Source |
|----------|---------|--------|
| V_IN     | 20–60 V | DC input connector |
| V_5V     | 5.0 V   | TPS54360 output |
| V_3V3    | 3.3 V   | AMS1117 output |
| GND      | 0 V     | Common ground |
| VBUS     | 5 V     | USB-C (programming only, not powering board) |

### Signal Nets
| Net Name       | From              | To              | Notes |
|----------------|-------------------|-----------------|-------|
| CAN_TX         | ESP32 GPIO5       | TJA1050 TXD     | 3.3V logic |
| CAN_RX         | TJA1050 RXD       | ESP32 GPIO4     | Via divider |
| CANH           | TJA1050 pin 7     | J2 connector    | Differential |
| CANL           | TJA1050 pin 6     | J2 connector    | Differential |
| ACS1_OUT       | ACS712 VIOUT      | ESP32 GPIO34    | Via divider |
| ACS2_OUT       | J4 VIOUT          | ESP32 GPIO35    | Via divider |
| ACS3_OUT       | J5 VIOUT          | ESP32 GPIO32    | Via divider |
| ACS4_OUT       | J6 VIOUT          | ESP32 GPIO33    | Via divider |
| ACS5_OUT       | J7 VIOUT          | ESP32 GPIO36    | Via divider |
| V_SENSE        | Batt divider      | ESP32 GPIO39    | Via divider |
| USB_DP         | USB-C             | CH340C          | With ESD |
| USB_DM         | USB-C             | CH340C          | With ESD |
| UART_TX        | CH340C TX         | ESP32 GPIO3     | (RX0) |
| UART_RX        | CH340C RX         | ESP32 GPIO1     | (TX0) |
| I2C_SDA        | J8 pin 3          | ESP32 GPIO26    | With pull-up |
| I2C_SCL        | J8 pin 4          | ESP32 GPIO27    | With pull-up |
| STATUS_LED     | ESP32 GPIO2       | LED via 1kΩ     | Green |

---

## 8. Connector Pinouts

### J1 — DC Power Input (2-pos screw terminal, 5.08mm)
| Pin | Signal | Notes |
|-----|--------|-------|
| 1   | V+     | 20–60 V DC positive |
| 2   | GND    | DC negative / ground |

### J2 — CAN Bus (3-pos screw terminal, 3.81mm)
| Pin | Signal | Notes |
|-----|--------|-------|
| 1   | CANH   | CAN High |
| 2   | CANL   | CAN Low |
| 3   | GND    | Signal ground / shield |

### J3 — Current Path (2-pos screw terminal, 5.08mm, 30A rated)
| Pin | Signal | Notes |
|-----|--------|-------|
| 1   | IP+    | Current in (from battery +) |
| 2   | IP-    | Current out (to load +) |

### J4–J7 — External ACS712 (3-pin headers, 2.54mm)
| Pin | Signal | Notes |
|-----|--------|-------|
| 1   | VCC    | 5V supply for module |
| 2   | GND    | Ground |
| 3   | VOUT   | Analog output (pre-divider on module, post-divider on PCB) |

### J8 — I2C Display (4-pin header, 2.54mm)
| Pin | Signal |
|-----|--------|
| 1   | GND    |
| 2   | 3V3    |
| 3   | SDA    |
| 4   | SCL    |

### J9 — USB-C (programming/debug)

---

## 9. Board Block Diagram

```
  ┌─────────────────────────────────────────────────────────────────────────┐
  │                        eBike Monitor PCB                                │
  │                                                                         │
  │  ┌──────────┐    ┌──────────┐    ┌──────────┐                          │
  │  │ J1: DC   │───▶│ Input    │───▶│ TPS54360 │──▶ 5V ──┬── TJA1050     │
  │  │ 20-60V   │    │ Protect  │    │ Buck     │         ├── ACS712      │
  │  └──────────┘    │ TVS+FET  │    │ Conv.    │         │               │
  │                  └──────────┘    └──────────┘         ▼               │
  │                                                  ┌──────────┐          │
  │  ┌──────────┐    ┌──────────────────────────┐    │AMS1117   │          │
  │  │ J2: CAN  │───▶│ TJA1050 CAN Transceiver  │    │3.3V LDO │          │
  │  │ Bus      │◀───│ + ESD + Termination       │    └────┬─────┘          │
  │  └──────────┘    └──────────┬───────────────┘         │               │
  │                             │ TX/RX                    ▼               │
  │                             ▼                    ┌──────────┐          │
  │  ┌──────────┐    ┌────────────────────────┐     │ ESP32    │          │
  │  │ J3: 30A  │───▶│ ACS712-30A On-Board     │────▶│ WROOM-32E│          │
  │  │ Current  │    │ + Divider + Filter      │     │          │          │
  │  └──────────┘    └────────────────────────┘     │  GPIO34  │          │
  │                                                  │  GPIO35  │◀── J4   │
  │  ┌──────────┐                                    │  GPIO32  │◀── J5   │
  │  │ J8: I2C  │◀───────────────────────────────────│  GPIO26  │          │
  │  │ Display  │                                    │  GPIO27  │          │
  │  └──────────┘                                    │          │          │
  │                                                  │  GPIO1/3 │          │
  │  ┌──────────┐    ┌──────────┐                    │  (UART)  │          │
  │  │ J9: USB-C│───▶│ CH340C   │───────────────────▶│          │          │
  │  │          │    │ USB-UART │                    │          │          │
  │  └──────────┘    └──────────┘                    └──────────┘          │
  │                                                                         │
  │  [SW1:RST] [SW2:BOOT] [LED:GPIO2] [JP1:CAN Term]                      │
  └─────────────────────────────────────────────────────────────────────────┘
```
