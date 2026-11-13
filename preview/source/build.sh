#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WASI_SDK_PATH:?Set WASI_SDK_PATH to an installed WASI SDK}"
wasm=$(mktemp)
trap 'rm -f "$wasm"' EXIT
"$WASI_SDK_PATH/bin/clang++" -std=c++11 -DARDUINO=10819 -O2 \
  -fno-exceptions -fno-rtti -mexec-model=reactor \
  -Ishim -Ivendor/gfx -Ivendor/ssd1306 \
  -Wl,--allow-undefined,--export=preview_setup,--export=preview_step,--export=preview_buffer,--export=preview_page,--export=preview_battle,--export=preview_tone \
  preview.cpp vendor/gfx/Adafruit_GFX.cpp vendor/ssd1306/Adafruit_SSD1306.cpp -o "$wasm"
python3 - "$wasm" "${1:-DSH-DEX-Preview.html}" <<'PY'
from base64 import b64encode
from pathlib import Path
import sys

template = Path("preview.html.in").read_text()
image = b64encode(Path(sys.argv[1]).read_bytes()).decode("ascii")
licenses = "\n\n".join(
    "Adafruit " + name + "\n" + (Path("vendor") / folder / "license.txt").read_text()
    for name, folder in (("GFX 1.12.1", "gfx"), ("SSD1306 2.5.13", "ssd1306"))
)
output = template.replace("__PREVIEW_WASM__", image)
Path(sys.argv[2]).write_text(output.replace("</html>", "<!--\n" + licenses + "\n-->\n</html>"))
PY
