#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="${0:A:h}"
cd "${SCRIPT_DIR}"

# SDL2 のバージョンを指定
SDL_VERSION="2.32.10"
SDL_PATCH="patches/sdl2-${SDL_VERSION}-android-compat.patch"

# ダウンロード
curl -fL -o "SDL2-${SDL_VERSION}.zip" \
  "https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL2-${SDL_VERSION}.zip"

# 解凍
rm -rf "SDL2-${SDL_VERSION}"
unzip -q "SDL2-${SDL_VERSION}.zip"

# Android 14+ receiver rules, Android 15 edge-to-edge, and runtime permissions.
patch --batch --forward -d "SDL2-${SDL_VERSION}" -p1 < "${SDL_PATCH}"
rm -f "SDL2-${SDL_VERSION}/android-project/app/src/main/java/org/libsdl/app/"*.orig

# 古いSDLファイルが混在しないように入れ替える
rm -rf app/jni/SDL/include app/jni/SDL/src
rm -rf app/src/main/java/org/libsdl/app

# jni/SDL に include と src をコピー
cp -R "SDL2-${SDL_VERSION}/include" app/jni/SDL/
cp -R "SDL2-${SDL_VERSION}/src" app/jni/SDL/

# Java ソースのコピー
cp -R "SDL2-${SDL_VERSION}/android-project/app/src/main/java/org/libsdl/app" \
      app/src/main/java/org/libsdl/

# Clean up
rm -f "SDL2-${SDL_VERSION}.zip"
rm -rf "SDL2-${SDL_VERSION}"
