#! /bin/bash

#
# sudo apt install build-essential libsdl2-dev
#

make clean
make

APP_NAME=XM8
VERSION=1.7.1
APP_ICON=AppIcon.png

rm -rf "./$APP_NAME/"
mkdir -p "./$APP_NAME"/usr/bin
mkdir "./$APP_NAME"/DEBIAN
mkdir -p "./$APP_NAME/usr/share/applications"
mkdir -p "./$APP_NAME/usr/share/icons/hicolor/512x512/apps"
cp control "./$APP_NAME/DEBIAN/"
sed -i -e "s/APP_NAME/$APP_NAME/g" "./$APP_NAME/DEBIAN/control"
sed -i -e "s/VERSION/$VERSION/g" "./$APP_NAME/DEBIAN/control"
mv ./xm8 "./$APP_NAME/usr/bin/xm8"
cp ./xm8.desktop "./$APP_NAME"/usr/share/applications/
convert AppIcon.png -resize 512x "./$APP_NAME"/usr/share/icons/hicolor/512x512/apps/xm8.png
INSTALLED_SIZE=$(du -s -k $APP_NAME | sed -e 's/$APP_NAME//g')
sed -i -e "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" "./$APP_NAME/DEBIAN/control"
fakeroot dpkg-deb --build "./$APP_NAME" .
rm -rf "./$APP_NAME/"