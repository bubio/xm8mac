#!/bin/zsh

APP_ICONSET=AppIcon.iconset
mkdir $APP_ICONSET
sips -s format png -z 16 16 $1 -s dpiHeight 72.0 -s dpiWidth 72.0 --out $APP_ICONSET/icon_16x16.png
sips -s format png -z 32 32 $1 -s dpiHeight 144.0 -s dpiWidth 144.0 --out $APP_ICONSET/icon_16x16@2x.png
sips -s format png -z 32 32 $1 -s dpiHeight 72.0 -s dpiWidth 72.0 --out $APP_ICONSET/icon_32x32.png
sips -s format png -z 64 64 $1 -s dpiHeight 144.0 -s dpiWidth 144.0 --out $APP_ICONSET/icon_32x32@2x.png
sips -s format png -z 128 128 $1 -s dpiHeight 72.0 -s dpiWidth 72.0 --out $APP_ICONSET/icon_128x128.png
sips -s format png -z 256 256 $1 -s dpiHeight 144.0 -s dpiWidth 144.0 --out $APP_ICONSET/icon_128x128@2x.png
sips -s format png -z 256 256 $1 -s dpiHeight 72.0 -s dpiWidth 72.0 --out $APP_ICONSET/icon_256x256.png
sips -s format png -z 512 512 $1 -s dpiHeight 144.0 -s dpiWidth 144.0 --out $APP_ICONSET/icon_256x256@2x.png
sips -s format png -z 512 512 $1 -s dpiHeight 72.0 -s dpiWidth 72.0 --out $APP_ICONSET/icon_512x512.png
sips -s format png -z 1024 1024 $1 -s dpiHeight 144.0 -s dpiWidth 144.0 --out $APP_ICONSET/icon_512x512@2x.png
python3 - "$APP_ICONSET" AppIcon.icns <<'PY'
import struct
import sys
from pathlib import Path

iconset = Path(sys.argv[1])
chunks = []
for chunk_type, filename in (
    ("icp4", "icon_16x16.png"),
    ("icp5", "icon_16x16@2x.png"),
    ("icp6", "icon_32x32@2x.png"),
    ("ic07", "icon_128x128.png"),
    ("ic08", "icon_128x128@2x.png"),
    ("ic09", "icon_256x256@2x.png"),
    ("ic10", "icon_512x512@2x.png"),
):
    image = (iconset / filename).read_bytes()
    chunks.append(chunk_type.encode("ascii") + struct.pack(">I", len(image) + 8) + image)

payload = b"".join(chunks)
Path(sys.argv[2]).write_bytes(b"icns" + struct.pack(">I", len(payload) + 8) + payload)
PY
rm -rf $APP_ICONSET
