# XM8 for macOS

XM8は、Windows/Linux/Android/macOSのマルチプラットフォームに対応したPC-8801MA
(PC-8801mkIISR上位互換)のエミュレータです。



このリポジトリは ＰＩ．さんから許可をいただき作成しています。

公式では配布されていないmacOS版の開発を主に行なっていきます。



公式はこちらです。

http://retropc.net/pi/xm8/index.html



## 動作環境

| CPU           | 最小OSバージョン    |
| ------------- | ------------------- |
| x86_64        | Mac OS X v10.7 Lion |
| Apple Silicon | macOS 11 Big Sur    |



## ROMファイル

使用できるROMファイルについては、[README-XM8.txt](./README-XM8.txt)の[ROMファイル]を参照してください。

### 配置場所

ROMファイルの配置場所は、設定ファイルと同じ以下になります（一度、アプリケーションを起動するとのフォルダが作成されます）。


```shell
~/Library/Application Support/retro_pc_pi/xm8
```




## 使用方法

[README-XM8.txt](./README-XM8.txt)の[使い方]を参照してください。



## ビルド方法



#### ビルド環境

- Xcode

  使うのはコマンドラインツールだけですが、Xcodeをインストールしてしまうのが手っ取り早いと思います。

- SDL2

  SDL2のインストールが必要です。

  [Homebrew](https://brew.sh/index_ja)が楽だと思います。



#### ビルド手順

プロジェクトのルートをターミナルで開き、以下のコマンドを実行します。

```shell
cd macOS
make
make package_app
```

これで、macOSフォルダに実行バンドルファイルが作成されているはずです。

XM8.appを'アプリケーション'フォルダに移動するなどして実行してください。



## 謝辞

ソースコードの改変を快諾してくださったＰＩ．氏にお礼申し上げます。