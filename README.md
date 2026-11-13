# DSH-DEX

A Pokémon virtual pet on a keychain. I made it for Dhruv because I wanted a Poké Ball you could carry around, open up, and use to battle a friend's. The case slides apart to reveal a small OLED and four buttons, all driven by a custom ESP32-C3 board.

**Build status:** The design files and firmware are ready. I'm still putting together the first physical unit and will add photos once it's built.

## How to play

Flash the Pokémon you want and flip on the side switch. Slide the red half up to see the screen and the white half down to reach the buttons.

- **Feed** gives your Pokémon a berry. **Pet** raises its happiness. **Select** opens its stats.
- **Battle** starts an IR match. Point the two units at each other and tap **Battle** on both. Shake during the three-second countdown to boost your attack.
- Hold **Battle** to practice solo. To charge over USB-C, flip the power switch off.

## Screenshots

### Full assembly

![Open CAD assembly showing the split shell, PCB, OLED, and buttons](enclosure/Open.png)

### PCB

![Routed two-layer PCB in KiCad](assets/renders/pcb-layout.png)

[See the populated board in 3D](assets/renders/DSH-DEX-THT-preview.png).

### Schematic

[![Full KiCad schematic](assets/renders/schematic.png)](assets/renders/schematic.png)

## Build files

- **Electronics.** [KiCad project](hardware/kicad/), [Gerbers](hardware/manufacturing/DSH-DEX-Gerbers.zip), [assembly files](hardware/manufacturing/Assembly/), and [board ordering instructions](hardware/manufacturing/README.md).
- **Parts.** [BOM with links and estimated costs](hardware/DSH-DEX-BOM.csv). The battery still needs a supplier quote.
- **Case.** Send the [seven separate STLs](enclosure/Print-Ready/DSH-DEX-Print-STLs.zip) to the print shop at 100% scale in millimeters, then follow the [assembly instructions](enclosure/Print-Ready/README.md). The [Fusion and STEP print layouts](enclosure/Print-Ready/) show the pieces side by side.
- **Firmware.** Pick [Garchomp #0445](firmware/Garchomp/), [Giratina #0487](firmware/Giratina/), or [Metagross #0376](firmware/Metagross/). Use the first-flash file for a blank board or the app file to switch Pokémon and keep your saves. [Flashing instructions](firmware/README.md) and [Arduino source](firmware/dsh_dex/).
- **Art.** [72 × 64 monochrome sprites](assets/sprites/) for all three Pokémon.

For the assembled Fusion model, PCB STEP, and KiCad 3D models, download the repository and run `sh restore-cad.sh` from its root.

# AI USAGE
This section covers how i used ai, so i mainly js used ai for bug fixing (35%~ of code) <--- ik that seems like a lot but im a shitty coder so there was a shit ton of bugs, i also used ai for uploading files since theres no upload limit for ai agents, and i also used it to make the assembly instruction better and more formal from the my current tone it is easier to understand.
