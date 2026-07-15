#!/bin/zsh

set -euo pipefail

SCRIPT_DIR="${0:A:h}"
REPO_ROOT="${SCRIPT_DIR}/../.."

# Install required tools
brew bundle install --file="${SCRIPT_DIR}/Brewfile"

pushd .
cd "${REPO_ROOT}"

# Remove previous artifact.
rm -rf build

# Allow optional architecture argument
if [ $# -ge 1 ]; then
  ARCHS="$1"
else
  ARCHS="x86_64;arm64"
fi

# cmake -G Xcode -S . -B build -DCMAKE_BUILD_TYPE=Release -DMACOSX_STANDALONE_APP_BUNDLE=ON
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMACOSX_STANDALONE_APP_BUNDLE=ON -DXM8_ENABLE_RETROACHIEVEMENTS=ON -DBUILD_TESTING=OFF -DCMAKE_OSX_ARCHITECTURES="${ARCHS}"
cmake --build build --parallel "$(sysctl -n hw.physicalcpu)" --target package

popd .
