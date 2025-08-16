#!/bin/zsh

# Install required tools
brew bundle install

pushd .
cd ../..

# Remove previous artifact.
rm -rf build

# Allow optional architecture argument
if [ $# -ge 1 ]; then
  ARCHS="$1"
else
  ARCHS="x86_64;arm64"
fi

# cmake -G Xcode -S . -B build -DCMAKE_BUILD_TYPE=Release -DMACOSX_STANDALONE_APP_BUNDLE=ON
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMACOSX_STANDALONE_APP_BUNDLE=ON -DCMAKE_OSX_ARCHITECTURES="${ARCHS}"
cmake --build build -j $(sysctl -n hw.physicalcpu) --target package

popd .