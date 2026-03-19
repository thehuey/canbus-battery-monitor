# PCB Layout Guidelines

## Board Specifications

| Parameter | Value |
|-----------|-------|
| Dimensions | 80 mm x 55 mm |
| Layers | 2 (Top + Bottom) |
| Thickness | 1.6 mm FR4 |
| Copper Weight | 2 oz (70 µm) — needed for ACS712 current path |
| Min Trace Width | 0.2 mm (8 mil) for signals |
| Min Clearance | 0.2 mm (8 mil) |
| Min Via | 0.3 mm drill / 0.6 mm pad |
| Solder Mask | Green (both sides) |
| Silkscreen | White (both sides) |
| Surface Finish | HASL (leaded) or ENIG |
| Mounting Holes | 4x M3 at corners, 3.5 mm from edge |

## PCB Manufacturer Targeting

Designed for **JLCPCB** or **PCBWay** standard capabilities:
- 2-layer, standard 1.6mm
- 2 oz copper (specify — default is 1 oz)
- Most SMD parts available from JLCPCB parts library (LCSC)
- Assembly: single-sided SMT (top side) recommended

---

## Zone Layout (Top View)

```
┌─────────────────────────────────────────────────────────────┐
│ H1                                                       H2 │
│                                                              │
│  ┌─────────┐  ┌─────────────────────────────┐               │
│  │ POWER   │  │      ESP32-WROOM-32E        │  ┌─────────┐ │
│  │ INPUT   │  │      (center of board)       │  │ USB-C   │ │
│  │ J1      │  │                              │  │ J9      │ │
│  │         │  │      Antenna overhang ───────│──│─────────│─┤→ Keep clear!
│  │ TPS54360│  │                              │  │ CH340C  │ │
│  │ + LDO   │  │                              │  │         │ │
│  └─────────┘  └─────────────────────────────┘  └─────────┘ │
│                                                              │
│  ┌─────────────┐  ┌───────────┐  ┌──────────────────────┐  │
│  │ ACS712-30A  │  │ TJA1050   │  │ External Headers     │  │
│  │ + J3 (30A)  │  │ + J2 CAN  │  │ J4  J5  J6  J7  J8  │  │
│  │ HIGH CURRENT│  │           │  │ (ACS ext + Display)  │  │
│  └─────────────┘  └───────────┘  └──────────────────────┘  │
│                                                              │
│ H3        [SW1] [SW2] [LED1] [JP1]                       H4 │
└─────────────────────────────────────────────────────────────┘
```

---

## Critical Layout Rules

### 1. ESP32 Antenna Keep-Out Zone

**This is the most important layout rule on the entire board.**

```
    ┌──────────────────────────────┐
    │  ESP32-WROOM-32E Module      │
    │                              │
    │  ┌────────┐                  │
    │  │Antenna │ ← KEEP-OUT ZONE │
    │  │(PCB    │   No copper      │
    │  │antenna)│   No traces      │
    │  │        │   No ground plane│
    │  └────────┘   No components  │
    │              within 15mm      │
    └──────────────────────────────┘
         ▲
         │
    Antenna must extend past
    board edge OR have full
    clearance (no copper/GND
    pour) for 15mm around it
```

- The ESP32 antenna is at one end of the module
- **No copper pour, traces, or components within 15 mm of the antenna**
- Best practice: let the antenna overhang the board edge
- Ground plane must have a cutout under the antenna area

### 2. Power Supply — TPS54360 Buck Converter

```
    Current Loop (minimize area!):

    TPS54360 VIN ← C1,C2 (input caps) → GND
         │
         ▼ (internal high-side FET)
    PH pin → L1 (inductor) → C3,C4 (output caps) → GND
         │
         ▼ (when FET off)
    D3 (Schottky) ← GND

    CRITICAL: The loop V_IN → TPS54360 → D3 → GND → C1/C2
    must be as tight as possible to minimize EMI.
```

**Specific rules:**
- Place C2 (100nF ceramic) directly at TPS54360 VIN pin, < 5mm trace
- Place D3 (Schottky) directly adjacent to PH pins and GND
- L1 connects from PH to output caps — keep this trace short and wide (1mm+)
- C3/C4 output caps directly at inductor output pad
- Ground plane under the entire converter area
- TPS54360 exposed pad must have multiple vias to ground plane (thermal + electrical)
- Feedback resistor divider (R_TOP, R_BOT) — route from output caps to FB pin, not from inductor side
- Keep compensation network (R_COMP, C_COMP) close to COMP pin

### 3. ACS712 High-Current Path

```
    J3 Pin 1 (IP+) ──────────────────── ACS712 Pin 1,2 (IP+)
    (30A trace)          WIDE TRACE
                         2.5mm min
                         Both layers
    J3 Pin 2 (IP-) ◀──────────────────── ACS712 Pin 3,4 (IP-)
```

- **Minimum 2.5 mm (100 mil) trace width** for the IP+/IP- current path
- Use **both copper layers** with stitching vias (at least 4 vias per transition)
- Keep the current path **short and straight** between J3 and ACS712
- No ground plane under the current path between IP+ and IP- (to avoid magnetic coupling)
- The sensing traces (VIOUT to voltage divider) must be **routed away** from the current path
- Thermal relief on IP± pads — these will carry the full load current

### 4. CAN Bus

- Route CANH and CANL as a **differential pair**, 0.2mm trace / 0.2mm gap
- Keep the pair together from TJA1050 to J2 connector
- Place D4 (PESD2CAN ESD) as close to J2 as physically possible
- Place C13 (bypass cap) directly at TJA1050 VCC pin
- Keep CAN traces away from the switching converter area
- 120Ω termination resistor R11 close to TJA1050, with JP1 jumper accessible at board edge

### 5. ADC Signal Routing

- Route all ADC traces (ACS712 VIOUT, voltage sense) **away from** the buck converter
- Place voltage divider resistors and filter caps **close to the ESP32 ADC pins**, not near the signal source
- Use a ground guard ring or ground trace alongside ADC signal traces if they must cross noisy areas
- Keep ADC traces on the **opposite side of the board** from the power inductor if possible
- All five ACS712 voltage dividers should have matched layout (equal trace lengths)

### 6. Ground Plane Strategy

```
    ┌─────────────────────────────────────────────┐
    │  SOLID GROUND PLANE (Bottom Layer)          │
    │                                              │
    │  Exceptions:                                 │
    │  - Cutout under ESP32 antenna (15mm zone)   │
    │  - Gap between ACS712 IP+ and IP- pads      │
    │                                              │
    │  Star ground connections:                    │
    │  - Power ground (TPS54360, C1, D3)          │
    │  - Digital ground (ESP32, CH340C)            │
    │  - Analog ground (ADC dividers, ACS712)     │
    │  All meet at a single point near the LDO    │
    └─────────────────────────────────────────────┘
```

- Bottom layer = **continuous ground plane** (with the above exceptions)
- Top layer = signal routing + power traces + component placement
- Connect all ground pins with short, wide traces to the ground plane via vias
- TPS54360 exposed pad: **minimum 9 thermal vias** (0.3mm drill) to ground plane

---

## Trace Width Reference

| Net | Current | Min Width | Recommended |
|-----|---------|-----------|-------------|
| V_IN (20-60V) | 1A max | 0.5 mm | 1.0 mm |
| V_5V | 500 mA | 0.3 mm | 0.8 mm |
| V_3V3 | 400 mA | 0.3 mm | 0.6 mm |
| ACS712 IP± | 30A | 2.5 mm | 3.0 mm + both layers |
| CAN (CANH/CANL) | Signal | 0.2 mm | 0.25 mm differential pair |
| ADC signals | Signal | 0.15 mm | 0.2 mm |
| USB D+/D- | Signal | 0.2 mm | Matched length pair |
| General signals | Signal | 0.15 mm | 0.2 mm |
| GND plane | — | Continuous | Continuous pour |

---

## Thermal Considerations

### Heat Sources
1. **TPS54360** — Worst case at 60V input, 500mA output:
   - P_loss ≈ V_IN × I_OUT × (1 - efficiency) ≈ 60 × 0.5 × 0.15 = 4.5 W
   - Needs good thermal pad connection to ground plane
   - Consider copper area on top layer around IC (thermal island)

2. **AMS1117-3.3** — Dropout from 5V to 3.3V at 400mA:
   - P_loss = (5.0 - 3.3) × 0.4 = 0.68 W
   - SOT-223 tab soldered to copper area (minimum 1 cm²)

3. **ACS712 IP± path** — At 30A:
   - Internal resistance ≈ 1.2 mΩ → P = 30² × 0.0012 = 1.08 W
   - Wide traces and thermal vias help dissipate

### Thermal Vias
- TPS54360 exposed pad: 9+ vias, 0.3mm drill, in a 3x3 grid
- AMS1117 tab pad: 4+ vias to ground plane
- ACS712 IP± pads: multiple vias to bottom copper for heat spreading

---

## Component Placement Checklist

- [ ] ESP32 antenna extends past board edge or has 15mm clearance
- [ ] Input caps C1/C2 within 5mm of TPS54360 VIN pin
- [ ] Schottky diode D3 directly adjacent to TPS54360 PH and GND pins
- [ ] Inductor L1 close to TPS54360 PH pin output
- [ ] Output caps C3/C4 at inductor output, before distribution
- [ ] Feedback divider between output caps and FB pin (not from inductor side)
- [ ] AMS1117 input cap C6 at LDO input pin
- [ ] TJA1050 bypass cap C13 at VCC pin
- [ ] CAN ESD diode D4 at J2 connector (not at TJA1050)
- [ ] USB ESD diode D5 at USB-C connector
- [ ] ACS712 bypass cap C14 at VCC pin 8
- [ ] ACS712 filter cap C_F at FILTER pin 6
- [ ] All ADC voltage dividers close to ESP32 GPIO pins
- [ ] Buttons SW1/SW2 accessible at board edge
- [ ] JP1 termination jumper accessible at board edge
- [ ] Mounting holes at corners with 3.5mm edge clearance

---

## Design Rule Check (DRC) Settings

For JLCPCB standard 2-layer:

```
Clearance:          0.2 mm (8 mil)
Track width min:    0.15 mm (6 mil)
Via drill min:      0.3 mm (12 mil)
Via pad min:        0.6 mm (24 mil)
Annular ring min:   0.13 mm (5 mil)
Silkscreen width:   0.15 mm min
Silkscreen clearance: 0.1 mm from pads
Board edge clearance: 0.3 mm
```

---

## Manufacturing Files Needed

When exporting from KiCad for JLCPCB:
1. **Gerber files** — all layers (F.Cu, B.Cu, F.Mask, B.Mask, F.Silk, B.Silk, Edge.Cuts)
2. **Drill files** — Excellon format, PTH and NPTH separate
3. **BOM** — CSV with LCSC part numbers (see BOM.csv)
4. **CPL (Component Placement List)** — X, Y, rotation for SMT assembly
5. Specify: 2 oz copper, HASL finish, green solder mask
