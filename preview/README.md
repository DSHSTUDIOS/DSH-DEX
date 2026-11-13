# DSH-DEX Garchomp UI

Open `DSH-DEX-Preview.html` in a browser. It runs offline and uses the same 128 × 64 OLED drawing code as the ESP32-C3 firmware.

Use **F** to feed, **P** to pet, **B** to open Battle, and **S** to cycle through Home, Care, and Hardware. Edit the sprite in the pixel grid and use **Export C++** to generate a replacement for `drawPet()` in `source/main.cpp`.

## Battle

- **Practice on a laptop or device.** Press **B**, then **F** for Practice. Use **F/P** to pick a rival and **B** to start. In battle, use **F/P** to cycle through moves and **B** to use one. **S** exits.
- **IR duel on two devices.** Point the devices at each other. From the Battle menu, press **P** on the host and **B** on the other device to join. Each player picks a move with **F/P** and confirms with **B**. The round resolves after both players confirm.
- Every battle starts with 60 HP per side and costs 15 pet energy and 8 fullness. Quick attacks are more accurate, power moves hit harder, and Guard reduces damage and counters an attack. Types affect damage. The turn log shows damage, misses, and immunity. A double knockout is a draw. After 24 rounds, the higher HP wins, with ties counted as draws.

The IR packet includes the sender’s National Pokédex ID, a match code, the message type, a turn number, the chosen move, and a CRC. Both devices compute the same battle outcome. The existing GPIO3 emitter and GPIO20 receiver use 38 kHz NEC-style pulse timing. [Vishay TSOP38238 datasheet](https://www.vishay.com/docs/82491/tsop382.pdf)

## Device firmware

Open this directory as a PlatformIO project. Run `pio run` to build or `pio run -t upload` to upload with the ESP32-C3 connected over USB. Garchomp (#0445) is the default. Set `-DDSH_DEX_SPECIES=25` for Pikachu or `-DDSH_DEX_SPECIES=6` for Charizard in `platformio.ini` under `build_flags`, alongside the existing flags. Those builds display the species name on the home screen. Add new species and their moves to the roster in `source/battle.h`.

## Rebuild the offline preview

From this directory, set `WASI_SDK_PATH` to your WASI SDK installation and run the build script.

```sh
./source/build.sh ../DSH-DEX-Preview.html
```
