#!/usr/bin/env bash
set -euo pipefail
set -x

BUILD_DIR="${1-build}"
cmake --install "$BUILD_DIR" --prefix "${BUILD_DIR}/AppDir/usr"

APPIMAGE_BUILDER="${APPIMAGE_BUILDER:-linuxdeploy-x86_64.AppImage}"
if ! which "$APPIMAGE_BUILDER"; then
	if ! [[ -f linuxdeploy-x86_64.AppImage ]]; then
		wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage -N
		chmod +x linuxdeploy-x86_64.AppImage
	fi
	APPIMAGE_BUILDER=./linuxdeploy-x86_64.AppImage
fi
"$APPIMAGE_BUILDER" --appimage-extract-and-run --appdir="$BUILD_DIR"/AppDir --custom-apprun=Linux/AppRun -d Linux/xm8.desktop -o appimage

mv XM8*.AppImage Linux/xm8_linux_x86_64.appimage
