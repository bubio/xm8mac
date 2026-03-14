#!/usr/bin/env bash
#
# Update version number across all build systems.
#
# Usage: ./scripts/update_version.sh <major> <minor> <patch>
# Example: ./scripts/update_version.sh 1 7 8
#

set -euo pipefail

if [ $# -ne 3 ]; then
    echo "Usage: $0 <major> <minor> <patch>"
    echo "Example: $0 1 7 8"
    exit 1
fi

MAJOR=$1
MINOR=$2
PATCH=$3

# Validate: single digits only (BCD constraint)
if ! [[ "$MAJOR" =~ ^[0-9]$ && "$MINOR" =~ ^[0-9]$ && "$PATCH" =~ ^[0-9]$ ]]; then
    echo "Error: major, minor, patch must each be a single digit (0-9)"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

VER_DOT="${MAJOR}.${MINOR}.${PATCH}"
VER_BCD=$(printf "0x%02X%X%X" "$MAJOR" "$MINOR" "$PATCH")
VER_ANDROID="${MAJOR}.${MINOR}${PATCH}"

echo "Updating version to ${VER_DOT} (BCD: ${VER_BCD}, Android: ${VER_ANDROID})"

# 1. CMakeLists.txt — set(PROJECT_VERSION X.Y.Z)
sed -i.bak -E "s/^(set\(PROJECT_VERSION )[0-9]+\.[0-9]+\.[0-9]+\)/\1${VER_DOT})/" \
    "$ROOT_DIR/CMakeLists.txt"

# 2. Source/UI/app.cpp — #define APP_VER 0xXYZ
sed -i.bak -E "s/^(#define APP_VER[[:space:]]+)0x[0-9A-Fa-f]+/\1${VER_BCD}/" \
    "$ROOT_DIR/Source/UI/app.cpp"

# 3. Builder/Windows/XM8.rc — FILEVERSION / PRODUCTVERSION (numeric)
sed -i.bak -E "s/^(FILEVERSION[[:space:]]+)[0-9]+,[0-9]+,[0-9]+,[0-9]+/\1${MAJOR},${MINOR},${PATCH},0/" \
    "$ROOT_DIR/Builder/Windows/XM8.rc"
sed -i.bak -E "s/^(PRODUCTVERSION[[:space:]]+)[0-9]+,[0-9]+,[0-9]+,[0-9]+/\1${MAJOR},${MINOR},${PATCH},0/" \
    "$ROOT_DIR/Builder/Windows/XM8.rc"

# 3b. XM8.rc — FileVersion / ProductVersion (string)
sed -i.bak -E "s/(VALUE \"FileVersion\",[[:space:]]+\")[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/\1${VER_DOT}.0/" \
    "$ROOT_DIR/Builder/Windows/XM8.rc"
sed -i.bak -E "s/(VALUE \"ProductVersion\",[[:space:]]+\")[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/\1${VER_DOT}.0/" \
    "$ROOT_DIR/Builder/Windows/XM8.rc"

# 4. Builder/Android/app/build.gradle — versionName
sed -i.bak -E "s/(versionName \")[0-9]+\.[0-9]+\"/\1${VER_ANDROID}\"/" \
    "$ROOT_DIR/Builder/Android/app/build.gradle"

# 5. Builder/Android/app/src/main/AndroidManifest.xml — android:versionName
sed -i.bak -E "s/(android:versionName=\")[0-9]+\.[0-9]+\"/\1${VER_ANDROID}\"/" \
    "$ROOT_DIR/Builder/Android/app/src/main/AndroidManifest.xml"

# 6. README.md — download URL version
sed -i.bak -E "s|(releases/download/)[0-9]+\.[0-9]+\.[0-9]+/|\1${VER_DOT}/|g" \
    "$ROOT_DIR/README.md"

# Clean up .bak files
find "$ROOT_DIR" -name "*.bak" -newer "$0" -delete 2>/dev/null || true

echo "Done. Updated files:"
echo "  CMakeLists.txt"
echo "  Source/UI/app.cpp"
echo "  Builder/Windows/XM8.rc"
echo "  Builder/Android/app/build.gradle"
echo "  Builder/Android/app/src/main/AndroidManifest.xml"
echo "  README.md"
