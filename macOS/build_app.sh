#!/bin/zsh

rm -rf build
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Debug
cmake --build build

FRAMEWORK_PATH=$HOME/Library/Frameworks
APP_NAME=XM8
VERSION=1.7.1
APP_ICON=AppIcon.png

rm -rf "./$APP_NAME.app/"
source iconcreate.sh $APP_ICON
mkdir -p "./$APP_NAME.app"/Contents/{Frameworks,MacOS,Resources}
# cp -R "$(FRAMEWORK_PATH)/SDL2.framework" "./$(APP_NAME).app/Contents/Resources/"
cp Info.plist "./$APP_NAME.app/Contents/"
sed -e "s/APP_NAME/$APP_NAME/g" -i "" "./$APP_NAME.app/Contents/Info.plist"
sed -e "s/VERSION/$VERSION/g" -i "" "./$APP_NAME.app/Contents/Info.plist"
mv ./build/xm8 "./$APP_NAME.app/Contents/MacOS/$APP_NAME"
mv ./AppIcon.icns "./$APP_NAME.app/Contents/Resources/"