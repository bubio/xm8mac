#!/usr/bin/env bash
set -euo pipefail
set -x

pushd .

cd ../..

# Remove previous artifact.
rm -rf build

# Build Exe
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCPACK=ON -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j 2

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
"$APPIMAGE_BUILDER" --appimage-extract-and-run --appdir="$BUILD_DIR"/AppDir --custom-apprun=Builder/Linux/AppRun -d Builder/Linux/xm8.desktop -o appimage

mv XM8*.AppImage Builder/Linux/XM8_Linux_x86_64.appimage

popd
