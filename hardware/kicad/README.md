# KiCad Project — eBike Battery Monitor PCB

## Quick Start

1. Open `ebike_monitor.kicad_pro` in **KiCad 7.0+**
2. Open the schematic editor and place components per `../SCHEMATIC.md`
3. Wire all nets per the net summary table in SCHEMATIC.md
4. Run ERC (Electrical Rules Check)
5. Assign footprints (most are specified in `../BOM.csv`)
6. Generate netlist → import into PCB editor
7. Place components per the zone layout in `../PCB_LAYOUT.md`
8. Route traces using the net class widths defined in the project
9. Add ground pour on B.Cu (bottom layer)
10. Run DRC, fix violations
11. Generate Gerber files to `gerber/` directory

## Net Classes (pre-configured)

| Class | Width | Clearance | Use |
|-------|-------|-----------|-----|
| Default | 0.2mm | 0.2mm | Signal traces |
| Power_VIN | 1.0mm | 0.5mm | 20-60V input rail |
| Power_5V | 0.8mm | 0.3mm | 5V rail |
| Power_3V3 | 0.6mm | 0.3mm | 3.3V rail |
| HighCurrent | 3.0mm | 0.5mm | ACS712 IP+/IP- (30A) |
| CAN_Bus | 0.25mm | 0.2mm | CANH/CANL differential pair |

## Key Symbols Needed

From KiCad standard libraries:
- `Espressif:ESP32-WROOM-32E`
- `Interface_CAN_LIN:TJA1050T`
- `Sensor_Current:ACS712xLCTR-30A`
- `Regulator_Switching:TPS54360`
- `Regulator_Linear:AMS1117-3.3`
- `Interface_USB:CH340C`
- `Connector_USB:USB_C_Receptacle_USB2.0`

## Manufacturing Target

- **JLCPCB** 2-layer, 2oz copper, green mask, HASL
- SMT assembly: top side only
- BOM and CPL files generated from KiCad BOM export
