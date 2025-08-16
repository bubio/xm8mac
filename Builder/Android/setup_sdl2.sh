#!/bin/zsh
set -euo pipefail

# SDL2 のバージョンを指定
SDL_VERSION="2.32.8"

# ダウンロード
curl -LO "https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL2-${SDL_VERSION}.zip"

# 解凍
unzip "SDL2-${SDL_VERSION}.zip"

# jni/SDL に include と src をコピー
cp -R "SDL2-${SDL_VERSION}/include" app/jni/SDL/
cp -R "SDL2-${SDL_VERSION}/src" app/jni/SDL/

# Java ソースのコピー
cp -R "SDL2-${SDL_VERSION}/android-project/app/src/main/java/org/libsdl/app" \
      app/src/main/java/org/libsdl/

# Clean up
rm -f "SDL2-${SDL_VERSION}.zip"
rm -rf "SDL2-${SDL_VERSION}"