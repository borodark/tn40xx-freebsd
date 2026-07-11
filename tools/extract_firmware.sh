#!/bin/sh
#
# extract_firmware.sh - Extract MV88X3120 PHY firmware from AKiTiO macOS kext
#
# The Marvell MV88X3120 PHY requires firmware to be uploaded during
# initialization. The firmware is not freely redistributable, but can
# be extracted from the AKiTiO macOS driver package.
#
# Usage: ./extract_firmware.sh [output_dir]
#
# Requires: curl, bsdtar, python3 (or python3.11)

set -e

OUTPUT_DIR="${1:-../sys/dev/tn40xx}"
WORKDIR=$(mktemp -d)
PKG_URL="https://www.akitio.com/attachments/article/293/tn40xx_10.12-1.51.pkg"
KEXT_SYMBOL_OFFSET=0xaaa80
KEXT_SYMBOL_END=0xea9a0

# Find python3
PYTHON=""
for p in python3 python3.11 python3.12 python3.9; do
    if command -v "$p" >/dev/null 2>&1; then
        PYTHON="$p"
        break
    fi
done
if [ -z "$PYTHON" ]; then
    echo "Error: python3 not found"
    exit 1
fi

echo "Downloading AKiTiO macOS driver..."
curl -sL -o "$WORKDIR/tn40xx.pkg" "$PKG_URL"

echo "Extracting kext from package..."
cd "$WORKDIR"
bsdtar xf tn40xx.pkg Payload
cat Payload | gzip -d 2>/dev/null | bsdtar xf - Library/Extensions/tn40xx.kext/Contents/MacOS/tn40xx

KEXT="Library/Extensions/tn40xx.kext/Contents/MacOS/tn40xx"
if [ ! -f "$KEXT" ]; then
    echo "Error: Failed to extract kext binary"
    rm -rf "$WORKDIR"
    exit 1
fi

echo "Extracting MV88X3120 firmware from kext binary..."
"$PYTHON" -c "
import struct, sys

with open('$KEXT', 'rb') as f:
    f.seek($KEXT_SYMBOL_OFFSET)
    fw_data = f.read($KEXT_SYMBOL_END - $KEXT_SYMBOL_OFFSET)

# Strip trailing zero words
while len(fw_data) > 2 and fw_data[-2:] == b'\x00\x00':
    fw_data = fw_data[:-2]
fw_data += b'\x00\x00'

n_words = len(fw_data) // 2
print(f'Firmware size: {len(fw_data)} bytes ({n_words} u16 words)')

outpath = sys.argv[1] + '/MV88X3120_phy.h'
with open(outpath, 'w') as out:
    out.write('''
#ifndef _MV88X3120_PHY_H
#define _MV88X3120_PHY_H

/*
 * Marvell MV88X3120 PHY firmware init data.
 * Extracted from AKiTiO tn40xx macOS kext v1.51.
 * Firmware version: 2.6.3.0
 */
static u16 MV88X3120_phy_initdata[] __initdata = {
''')
    for i in range(0, len(fw_data), 2):
        val = struct.unpack_from('<H', fw_data, i)[0]
        comma = ',' if i + 2 < len(fw_data) else ''
        out.write(f'  0x{val:04x}{comma}\n')
    out.write('};\n')
    out.write(f'unsigned int MV88X3120_phy_initdata_len = {len(fw_data)};\n')
    out.write('#endif\n')

print(f'Generated {outpath}')
" "$OUTPUT_DIR"

cd /
rm -rf "$WORKDIR"
echo "Done. MV88X3120_phy.h written to $OUTPUT_DIR/"
