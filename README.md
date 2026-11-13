# DSH-DEX
A portable pokemon themed tomogatchi. Made with :heart: for Dhruv 

### *still in assembly stage*

so basically i wanted to make a little pokemon pet that fits on a keychain. you slide the red half up to see the screen and the white half down to get to the buttons. you can feed it, pet it, shake it to power up an attack, and battle someone else's dex over IR.

## build files

- **pcb** — [KiCad files](hardware/kicad/), [Gerbers](hardware/manufacturing/DSH-DEX-Gerbers.zip), and [assembly files](hardware/manufacturing/Assembly/). im getting mine assembled. the [order instructions](hardware/manufacturing/README.md) have the settings and what to ask for.
- **bom** — [the parts list with links and prices](hardware/DSH-DEX-BOM.csv). the prices are estimates. the battery still needs a supplier quote.
- **case** — [seven separate STLs](enclosure/Print-Ready/DSH-DEX-Print-STLs.zip) and [assembly instructions](enclosure/Print-Ready/README.md). send the seven files to the print shop at 100% scale in mm, then put the pcb, battery and magnets in yourself. the [separated Fusion and STEP files](enclosure/Print-Ready/) are there too.
- **firmware** — flash whichever pokemon you want: [Garchomp #0445](firmware/Garchomp/), [Giratina #0487](firmware/Giratina/), or [Metagross #0376](firmware/Metagross/). use the first-flash file for a blank board and the app file if youre switching pokemon and want to keep your saves. [flash instructions](firmware/README.md) / [source](firmware/dsh_dex/)
- **sprites** — [72 x 64 monochrome pngs](assets/sprites/) for all three.

![Open split Poké Ball enclosure](enclosure/Open.png)

## restore the cad files

if you want the original assembled model or the pcb in 3d, download the repo and run `sh restore-cad.sh` from the repo folder. it'll put the Fusion file in `enclosure/` and the board STEP and 3d models in `hardware/`.

## where its at rn

Nothing is built yet, once the buildstage is finished, ill add all the steps with actual photos, from the physical project.

# AI USAGE
This section covers how i used ai, so i mainly js used ai for bug fixing (35%~ of code) <--- ik that seems like a lot but im a shitty coder so there was a shit ton of bugs, i also used ai for uploading files since theres no upload limit for ai agents, and i also used it to make the assembly instruction better and more formal from the my current tone it is easier to understand.
