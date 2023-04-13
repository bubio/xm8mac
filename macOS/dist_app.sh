#!/bin/zsh

# Install required tools
brew bundle install

# Remove previous artifact.
rm -rf build

# Create icon set
# source iconcreate.sh AppIcon.png

cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DMACOSX_STANDALONE_APP_BUNDLE=ON
cmake --build build -j $(sysctl -n hw.physicalcpu)