#!/bin/sh
set -eu
cd "$(dirname "$0")"

staging=$(mktemp -d)
trap 'rm -rf "$staging"' EXIT
trap 'exit 1' HUP INT TERM
cat hardware/model-parts/hardware-models.tar.xz.part-* > "$staging/hardware-models.tar.xz"
cat enclosure/fusion-parts/DSH-DEX-Split-Pokeball.f3d.part-* > "$staging/DSH-DEX-Split-Pokeball.f3d"
cp asset-checksums.sha256 "$staging/"
(
    cd "$staging"
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 -c asset-checksums.sha256
    else
        sha256sum -c asset-checksums.sha256
    fi
)
tar -xJf "$staging/hardware-models.tar.xz" -C hardware
mv "$staging/DSH-DEX-Split-Pokeball.f3d" enclosure/
