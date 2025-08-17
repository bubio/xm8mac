#!/bin/bash

#
# Install required tools
#

# Debian and Ubuntu
# sudo apt install build-essential cmake libsdl2-dev

# Fedora
# for dnf4
#   sudo dnf groupinstall "Development Tools"
#   sudo dnf install cmake gcc-c++ rpm-build SDL2-devel
# for dnf5
#   sudo dnf install @development-tools
#   sudo dnf install cmake  gcc-c++ rpm-build SDL2-devel

pushd .
cd ../../

# Remove previous artifact.
rm -rf build

cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCPACK=ON -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j 2 --target package

if [ -f build/xm8*.rpm ]; then
    mv build/xm8*.rpm build/XM8_Linux_x86_64.rpm
    
    read -p "Do you want to install the rpm package? (y/N): " yn
    case "$yn" in
        [yY]*)
            sudo dnf install ./build/XM8_Linux_x86_64.rpm
            ;;
        *)
            ;;
    esac    
fi

if [ -f build/xm8*amd64.deb ]; then
    mv build/xm8*amd64.deb build/XM8_Linux_x86_64.deb

    read -p "Do you want to install the deb package? (y/N): " yn
    case "$yn" in
        [yY]*)
            sudo apt install ./build/XM8_Linux_x86_64.deb
            ;;
        *)
            ;;
    esac
fi

if [ -f build/xm8*armhf.deb ]; then
    mv build/xm8*armhf.deb build/XM8_Linux_armhf.deb

    read -p "Do you want to install the deb package? (y/N): " yn
    case "$yn" in
        [yY]*)
            sudo apt install ./build/XM8_Linux_armhf.deb
            ;;
        *)
            ;;
    esac
fi

popd
