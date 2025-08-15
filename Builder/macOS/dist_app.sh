#!/bin/zsh

# Install required tools
brew bundle install

pushd .
cd ../..

# Remove previous artifact.
rm -rf build

# cmake -G Xcode -S . -B build -DCMAKE_BUILD_TYPE=Release -DMACOSX_STANDALONE_APP_BUNDLE=ON
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMACOSX_STANDALONE_APP_BUNDLE=ON
# codesign --sign - --timestamp=none build/xm8.app
xattr -rc build/xm8.app
cmake --build build -j $(sysctl -n hw.physicalcpu) --target package

popd .