# BOM — Engine Simulator 2.0

Bill of Materials automatically extracted from the KiCad schematic netlist.
**Update on every schematic/PCB change** (regenerate from the netlist).

- Project: `Hardware/Engine Simulator 2.0/Engine Simulator 2.0.kicad_sch`
- Last extraction: 2026-09-21
- Total components: **58**
- Format: TH (through-hole) — except the ESP32 (pre-assembled DevKitC module)

| Ref | Qty | Value | Footprint | Notes |
|:----|:---:|:------|:----------|:------|
| C1 | 1 | 0.33 uF | Capacitor_THT:C_Disc_D10.5mm_W5.0mm_P5.00mm | Power input filter |
| C2, C3, C6 | 3 | 0.1 uF | Capacitor_THT:C_Disc_D10.5mm_W5.0mm_P5.00mm | Local decoupling |
| C4 | 1 | 470 uF | PCM_Capacitor_THT_AKL:CP_Radial_D5.0mm_P2.00mm | 12V input bulk |
| C5 | 1 | 47 uF | PCM_Capacitor_THT_AKL:CP_Radial_D5.0mm_P2.00mm | 5V output bulk |
| C7 | 1 | 470 uF 16V | PCM_Capacitor_THT_AKL:CP_Radial_D5.0mm_P2.00mm | After D13 (reverse protection) |
| D13 | 1 | 1N5817 | Diode_THT:D_DO-41_SOD81_P10.16mm_Horizontal | Schottky diode, reverse polarity protection |
| DZ1–DZ9 | 9 | BZX55-C3V6 | PCM_Diode_THT_AKL:D_DO-35_SOD27_P7.62mm_Horizontal_Zener | 3.6V Zener, CD4050 input protection |
| J1 | 1 | ~ | Connector_IDC:IDC-Header_2x20_P2.54mm_Latch_Vertical | 40-pin IDC connector to Speeduino |
| J2 | 1 | DC IN | TerminalBlock_Altech:Altech_AK100_1x02_P5.00mm | 12V power terminal block |
| J3–J9, J11 | 8 | ~ | TerminalBlock_Altech:Altech_AK100_1x02_P5.00mm | Output/potentiometer terminal blocks |
| J10 | 1 | CRANK | TerminalBlock_Altech:Altech_AK100_1x02_P5.00mm | Crank signal terminal block |
| J12 | 1 | Screw_Terminal_01x02 | TerminalBlock_Altech:Altech_AK100_1x02_P5.00mm | Terminal block |
| R13, R14, R16, R18, R21 | 5 | 10 kOhm | Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal | Dividers / pull-ups |
| R15, R17 | 2 | 5.1 kOhm | Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal | Dividers |
| R22 | 1 | 20 kOhm | Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal | Fan input divider |
| R23–R31 | 9 | 2.2 kOhm | Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal | CD4050 series-R protection network (9 channels) |
| RV1, RV2 | 2 | 2 kOhm | Potentiometer_THT:Potentiometer_ACP_CA6-H2,5_Horizontal | Trimmers |
| RV3–RV5 | 3 | 10 kOhm | Potentiometer_THT:Potentiometer_ACP_CA9-V10_Vertical | TPS/IAT/O2 trimmers |
| SW2 | 1 | Start/Stop | Button_Switch_THT:SW_PUSH_1P1T_6x3.5mm_H5.0_APEM_MJTP1250 | Engine start/stop button |
| U1 | 1 | ESP32-DevKitC | PCM_Espressif:ESP32-DevKitC | Main microcontroller |
| U2 | 1 | 74AHCT125 | Package_DIP:DIP-14_W7.62mm_LongPads | Quad level shifter 3.3V->5V (Crank/Cam1/Cam2) |
| U3 | 1 | R-78E5.0-1.0 | Converter_DCDC:Converter_DCDC_RECOM_R-78E-0.5_THT | 5V/1A switching regulator, 7805 drop-in |
| U4 | 1 | TLV2372 | Package_DIP:DIP-8_W7.62mm_Socket_LongPads | Dual rail-to-rail op-amp (MAP/CLT buffers) |
| U5, U6 | 2 | CD4050BE | Package_DIP:DIP-16_W7.62mm_LongPads | Hex buffer for ECU inputs (4 inj + 4 ign + fan) |

## Notes

- **U3**: the `-0.5_THT` footprint is used as a drop-in for the 1.0A version (same SIP3 package, identical
  pinout, 1.5x2.3 mm pads). Recommended supplier: Conrad.it (~4.79 EUR). RS Italia (code 144-6290) unavailable.
- **U2**: a single 74AHCT125 chip (4 buffers) covers Crank, Cam1, Cam2 and one spare.
- **U4**: a single TLV2372 chip (2 op-amps) covers the MAP and CLT buffers.
- **DZ1–DZ9 + R23–R31**: protection network (3.6V Zener clamp) before the CD4050 inputs, 9 channels.
- **Connectors**: J1 = 40-pin IDC to Speeduino; 5.00 mm screw terminal blocks for external wiring.
