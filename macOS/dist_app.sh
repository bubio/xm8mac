#!/bin/zsh

# Install required tools
brew bundle install

# Remove previous artifact.
rm -rf build

cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DMACOSX_STANDALONE_APP_BUNDLE=ON
cmake --build build -j $(sysctl -n hw.physicalcpu)