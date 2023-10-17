# XM8 for macOS

---
[![Downloads](https://img.shields.io/github/downloads/bubio/xm8mac/total.svg)](https://github.com/bubio/xm8mac/releases/latest)

## XM8 for macOSとは
---
XM8は、ＰＩ．さんが開発したマルチプラットフォーム（Windows/Linux/Android）に対応したPC-8801MA(PC-8801mkIISR上位互換)のエミュレータをmacOS用の改変したものです。

<p align="center">
<img width="752" src="https://user-images.githubusercontent.com/78299054/226607145-c6381e6e-acb8-4fba-88dc-0a8462298f6e.png">
</p>

このリポジトリは ＰＩ．さんから許可をいただき作成しています。

公式では配布されていないmacOS版の開発を主に行なっていきますが、Windows/Linux/Android版もできる配布します。

<br />

公式はこちらです。

http://retropc.net/pi/xm8/index.html


<br />

## インストール方法
---

[リリース](https://github.com/bubio/xm8mac/releases)からお手持ちの環境にあった実行ファイルをダウンロードしてください。

`XM8.app`を`アプリケーション`フォルダに移動するなどして実行してください。

<br />

### 動作環境

| CPU           | 最小OSバージョン    | 実行ファイル                                                 |
| ------------- | ------------------- | ------------------------------------------------------------ |
| x86_64        | macOS 10.13 High Sierra | [x86_64版](https://github.com/bubio/xm8mac/releases/download/1.7.3/XM8_macOS_x86_64.dmg) |
| Apple Silicon | macOS 11 Big Sur    | [Apple Silicon版](https://github.com/bubio/xm8mac/releases/download/1.7.3/XM8_macOS_AppleSilicon.dmg) |

<br />

## ROMファイル
---
使用できるROMファイルについては、[README-XM8.txt](Documents/README-XM8.txt)の[ROMファイル]を参照してください。

<br />

### 配置場所
ROMファイルの配置場所は、設定ファイルと同じ以下になります（一度、アプリケーションを起動するとフォルダが作成されます）。


```shell
"~/Library/Application Support/retro_pc_pi/xm8"
```



<br />

## 使用方法
---
[README-XM8.txt](Documents/README-XM8.txt)の[使い方]を参照してください。


<br />

## ビルド方法
---

### ビルド環境

ビルドするには以下のインストールが必要です。

- Xcode

  使うのはコマンドラインツールだけですが、Xcodeをインストールしてしまうのが手っ取り早いと思います。

- Homebrew
  
  [Homebrew](https://brew.sh/index_ja)のインストールが必要です。
  cmskeなどビルドに必要なツールの取得に使用します。

<br />

### ビルド手順

プロジェクトのルートをターミナルで開き、以下のコマンドを実行します。

```shell
cd Builder/macOS
./dist_app.sh
```

これで、macOSフォルダに実行ファイル(.app)が作成されているはずです。

<br />


他のプラットフォーム用のビルドについては、[README-BUILD.txt](Documents/README-BUILD.txt)を参照してください。



<br />

## 謝辞
---
ソースコードの改変を快諾してくださったＰＩ．氏にお礼申し上げます。



<br />

## おまけ

---

### Windows版

----

Builder/WindowsフォルダにVisual Studio 2022用のソリューションが入っています。

<br />

SDL2のWindows (Visual C++ 32bit/64bit)向けライブラリ、ソースファイルをダウンロードします。
https://www.libsdl.org

<br />

これをSource\Windows\SDLへ展開します。以下のようになります。

Builder\Windows\SDL\include（インクルードファイル）
Builder\Windows\SDL\lib\x86（32bit向けライブラリ）
Builder\Windows\SDL\lib\x64（64bit向けライブラリ）

<br />

BIOS ROMファイルの置き場所は以下になります。

```shell
%appdata%\retro_pc_pi\xm8
```



### Linux版

----

Builder/Linuxフォルダにdeb, rpm, appimageパッケージを作成するスクリプトが入っています。

### deb or rpm
```shell
cd Builder/Linux
./dist_app.sh
```
これでbuildフォルダにdebファイル、またはrpmファイルが作成されます。

<br />

### appimage
```shell
cd Builder/Linux
./appimage.sh
```
これでBuilder/Linuxフォルダに、appimageファイルが作成されます。

<br />

BIOS ROMファイルの置き場所は以下になります。

```shell
~/.local/share/retro_pc_pi/xm8/
```



### Android版

----

Builder/AndroidフォルダにAndroid Studio用のプロジェクトが入っています。

<br />

SDL2のソースファイルをダウンロードします。
https://www.libsdl.org

<br />

Builder/Android/app/jni/SDL

にSDL2のsrcフォルダ、includeフォルダをコピーします。以下のようになります。

Builder/Android/app/jni/SDL\include（インクルードファイル）
Builder/Android/app/jni/SDL\src（ソースファイル）

<br />

BIOS ROMファイルの置き場所は以下になります。

```shell
Android/data/retro_pc_pi/files/
```

Android 11以上の場合、端末内のファイルに自由にアクセスすることができません。

ゲームのディスクイメージも同じ場所に入れることを推奨します。


## ライセンス

- xBRZ
  
  https://sourceforge.net/projects/xbrz/

  GNU General Public License version 3.0 (GPLv3)
