# DSH-DEX manufacturing files

Through-hole OLED revision. Single 47.0 × 46.6 mm PCB, approximately.

## Bare board order

Upload **DSH-DEX-Gerbers.zip** as-is. It contains the board outline, both copper, solder mask, silkscreen and solder paste layers, and separate plated and non-plated drill files. Set the order to **2-layer FR-4, 1.0 mm thickness, 1 oz outer copper, lead-free HASL**. Confirm the 1.0 mm thickness because the case fit and STEP assembly use that dimension. In the Gerber preview, check the outline, the antenna notch, the USB-C slots, and the four plated OLED holes.

## Assembled board order

Upload the same Gerber ZIP, then **Assembly/DSH-DEX-BOM.csv** and **Assembly/DSH-DEX-CPL.csv**. Select **Standard PCBA, both sides**. JLCPCB lists the ESP32 module C2838502 for Standard PCBA and the TSOP38238 receiver U5 for through-hole wave soldering.

The BOM and CPL cover 58 placements, including U5. Review the assembly preview for every part, especially U1, U4, U6, J1, SW1, D1, D2, and U5. Compare pin 1, polarity, and connector direction against the assembly drawings and board pads. The U5 lens faces the display. Confirm the supplier can assemble U5 and check part availability in the order.

R16 uses 43 Ω, 0805, 1%, 125 mW, UNI-ROYAL 0805W8F430JT5E (LCSC C4368). If it cannot be sourced by JLCPCB, the documented alternative is 43.2 Ω Yageo RC0805FR-0743R2L (JLCPCB C483157), with the same package, tolerance and power rating. The earlier project BOM listed C42000, which is 430 Ω.

## Final assembly

- Provide the 0.96-inch SSD1306 OLED module to the assembler and request hand installation after reflow. Its male header must match **GND, VCC, SCL, SDA** from pin 1 to pin 4. Seat it directly into J3, solder all four pins on the underside, and trim the tails to approximately 1 mm below the PCB. J3 uses four 0.90 mm plated holes at 2.54 mm pitch.
- Supply the battery separately. Use a protected single-cell 4.20 V LiPo with a compatible JST-SH plug, verified polarity, and a manufacturer-rated charge current of at least 112 mA.
- The OLED and battery are listed in **Reference/DSH-DEX-Full-BOM.csv**. The Reference folder also contains placement drawings, an IPC-D-356 electrical test netlist, and KiCad checks. The bottom drawing is viewed from the bottom side.
- KiCad schematic ERC and board DRC, including schematic parity, report zero violations. Check the supplier's placement preview and review a prototype before ordering a production quantity.

## Supplier references

- [JLCPCB fabrication exports](https://jlcpcb.com/help/article/how-to-generate-gerber-and-drill-files-in-kicad-7)
- [JLCPCB BOM and CPL upload format](https://jlcpcb.com/help/article/how-to-generate-the-bom-and-centroid-file-from-kicad)
- [JLCPCB ESP32-C3-MINI-1-N4](https://jlcpcb.com/partdetail/EspressifSystems-ESP32_C3_MINI_1N4/C2838502)
- [JLCPCB TSOP38238](https://jlcpcb.com/partdetail/VishayIntertech-TSOP38238/C141632)
- [LCSC 43 Ω resistor](https://www.lcsc.com/de/product-detail/Chip-Resistor-Surface-Mount_UNI-ROYAL-Uniroyal-Elec_C4368.html)
- [JLCPCB 43.2 Ω alternate](https://jlcpcb.com/partdetail/YAGEO-RC0805FR0743R2L/C483157)
