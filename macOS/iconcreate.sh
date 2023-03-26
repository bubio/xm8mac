#!/usr/bin/zsh

APP_ICONSET=AppIcon.iconset
mkdir $APP_ICONSET
sips -s format png -z 16 16 $1 -s dpiHeight 72.0 -s dpiWidth 72.0 --out $APP_ICONSET/icon_16.png
sips -s format png -z 32 32 $1 -s dpiHeight 144.0 -s dpiWidth 144.0 --out $APP_ICONSET/icon_16@2x.png
sips -s format png -z 32 32 $1 -s dpiHeight 72.0 -s dpiWidth 72.0 --out $APP_ICONSET/icon_32.png
sips -s format png -z 64 64 $1 -s dpiHeight 144.0 -s dpiWidth 144.0 --out $APP_ICONSET/icon_32@2x.png
sips -s format png -z 128 128 $1 -s dpiHeight 72.0 -s dpiWidth 72.0 --out $APP_ICONSET/icon_128.png
sips -s format png -z 256 256 $1 -s dpiHeight 144.0 -s dpiWidth 144.0 --out $APP_ICONSET/icon_128@2x.png
sips -s format png -z 256 256 $1 -s dpiHeight 72.0 -s dpiWidth 72.0 --out $APP_ICONSET/icon_256.png
sips -s format png -z 512 512 $1 -s dpiHeight 144.0 -s dpiWidth 144.0 --out $APP_ICONSET/icon_256@2x.png
sips -s format png -z 512 512 $1 -s dpiHeight 72.0 -s dpiWidth 72.0 --out $APP_ICONSET/icon_512.png
sips -s format png -z 1024 1024 $1 -s dpiHeight 144.0 -s dpiWidth 144.0 --out $APP_ICONSET/icon_512@2x.png
iconutil -c icns $APP_ICONSET
rm -rf $APP_ICONSET