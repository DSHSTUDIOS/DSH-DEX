# DSH-DEX Firmware Editions

Flash the Pokémon you want on your DSH-DEX PCB.

| Edition | Pokédex | IR species code |
| --- | ---: | ---: |
| Garchomp | #0445 | 1 |
| Giratina (Altered Forme) | #0487 | 4 |
| Metagross | #0376 | 5 |

Each edition has its own OLED sprite, Pokédex heading, IR identity, and save slot. Feed, Pet, Scroll, tilt, shake, and the short-press IR and long-press practice battles use the Garchomp sketch's controls and rules. Giratina and Metagross use a one-pixel idle bob. Their 72-pixel-wide sprites fit beneath the existing HUD.

## Flash over USB-C

Install esptool 4.9 with `python3 -m pip install esptool==4.9.0` in your Python environment. Connect the board with a USB-C data cable and turn the power switch on. Hold **BOOT**, tap **RESET**, then release **BOOT**. Find the port with `ls /dev/cu.usbmodem*` on macOS and replace the example port below. Tap **RESET** if the new firmware does not start automatically.

**Blank board:** Use the chosen edition's `-first-flash.bin` at `0x0`. This writes the bootloader, partition table, and application. Use this command only for a blank board or when you intend to replace existing progress.

```sh
python3 -m esptool --chip esp32c3 --port /dev/cu.usbmodemXXXX write_flash 0x0 Giratina/Giratina-first-flash.bin
```

**Switch editions on an existing DSH-DEX board:** Use the chosen edition's `-app.bin` at `0x10000`. This writes only the application and preserves the NVS save area. Switching back restores that Pokémon's progress. Garchomp uses the existing Garchomp save key and can still import the original pet save.

```sh
python3 -m esptool --chip esp32c3 --port /dev/cu.usbmodemXXXX write_flash 0x10000 Metagross/Metagross-app.bin
```

Use these instructions with the original DSH-DEX partition layout. Update older units to this release before using them in cross-edition IR battles so they recognize Giratina and Metagross.

## Build from source

Open the `dsh_dex` folder as a PlatformIO project and run `pio run` to build all three editions, or `pio run -e giratina` to build one. The sketch defaults to Garchomp when opened in Arduino IDE. PlatformIO keeps the same GPIO mapping and targets Arduino-ESP32 2.0.17.

All three images compiled for ESP32-C3. The host simulation verified sprite rendering, edition-specific save keys, and recognition of the new IR IDs. Physical flashing and optical battles on the assembled PCB still need testing.
