# DSH-DEX
A portable pokemon themed tomogatchi. Made with :heart: for Dhruv 

DSH-DEX is an ESP32-C3 virtual pet in a sliding Poké Ball keychain. It uses a 128 × 64 monochrome OLED, four gameplay buttons, motion input, a piezo buzzer, and IR battles between units.

| What you want to do | Where to start |
| --- | --- |
| Order the assembled PCB | [Manufacturing instructions](hardware/manufacturing/README.md), [Gerber ZIP](hardware/manufacturing/DSH-DEX-Gerbers.zip), [assembly BOM and placement files](hardware/manufacturing/Assembly/) |
| Budget and source the whole build | [Linked BOM with cost estimates](hardware/DSH-DEX-BOM.csv) |
| Edit the circuit or board | [KiCad project](hardware/kicad/DSH-DEX.kicad_pro), [schematic](hardware/kicad/DSH-DEX.kicad_sch), [PCB](hardware/kicad/DSH-DEX.kicad_pcb) |
| Print the split enclosure | [Print-shop ZIP](enclosure/DSH-DEX-Split-Print-Shop.zip), [individual STLs](enclosure/Print-STLs/), [assembly notes](enclosure/Assembly-notes.md) |
| Open the Fusion model or view the PCB in 3D | [Restore the CAD files](#restore-the-cad-files) |
| Flash a Pokémon | [Firmware instructions](firmware/README.md), [Garchomp #0445](firmware/Garchomp/), [Giratina #0487](firmware/Giratina/), [Metagross #0376](firmware/Metagross/), [Arduino source](firmware/dsh_dex/) |
| Try the game on a laptop | [Offline browser preview](preview/DSH-DEX-Preview.html). Download the HTML and open it locally. It runs the earlier Garchomp UI firmware. |
| Download the OLED art | [72 × 64 monochrome sprites](assets/sprites/) |

![Open split Poké Ball enclosure](enclosure/Open.png)

## Restore the CAD files

Download or clone this repository. From its root, run `sh restore-cad.sh` on macOS, Linux, or Windows with WSL. The script joins the archived parts, verifies both SHA-256 checksums, then restores these files:

- `hardware/kicad/3dmodels/` for the KiCad 3D viewer
- `hardware/DSH-DEX-THT-assembly.step` for the assembled PCB
- `enclosure/DSH-DEX-Split-Pokeball.f3d` for editing in Fusion

## Prototype status

The PCB's ERC and DRC reports are in the manufacturing folder. The three device firmware editions compiled and passed host tests. An assembled prototype still needs battery and enclosure fit checks and a physical IR link test. The BOM includes a battery supplier quote line with the required dimensions and ratings.
