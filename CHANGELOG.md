# 1.7.6

2025年8月17日 日曜日

<br/>　

## 動作環境
| Platform | CPU | 最小OSバージョン |
| ------------- | ------------------- | ------------- |
| Mac | x86_64 | macOS 10.13 High Sierra |
| Mac | Apple Silicon | macOS 11 Big Sur |
| Windows | x86_64 | Windows 10 |
| Windows | x86_32 | Windows 10 |
| Linux | x86_64 | - |
| Raspberry Pi OS | armhf | -|
| Android | Arm | Android 4.4 |

<br />　

## 変更点
- 機能的な変更はありません。
- macOSの形式をUniversal Binaryに変更しました。
- macOS版がmacOS 13以上となっていた問題を修正しました。

<br />　

## 既知の問題
- 特にありません。

<br/>　
<br/>　

# 1.7.5

2025年7月27日 日曜日

<br/>　

## 動作環境
| Platform | CPU | 最小OSバージョン |
| ------------- | ------------------- | ------------- |
| Mac | x86_64 | macOS 10.13 High Sierra |
| Mac | Apple Silicon | macOS 11 Big Sur |
| Windows | x86_64 | Windows 10 |
| Windows | x86_32 | Windows 10 |
| Linux | x86_64 | - |
| Raspberry Pi OS | armhf | -|
| Android | Arm | Android 4.4 |

<br />　

## 変更点
- SDLを2.32.8にしました。
- Androidのビルド環境をSDK 35にしました。
- MacのApple Silicon版が動作しない問題を修正しました。

<br />　

## 既知の問題
- 特にありません。

<br/>　
<br/>　

# 1.7.4

2023年10月23日 月曜日

<br/>　

## 動作環境
| Platform | CPU | 最小OSバージョン |
| ------------- | ------------------- | ------------- |
| Mac | x86_64 | macOS 10.13 High Sierra |
| Mac | Apple Silicon | macOS 11 Big Sur |
| Windows | x86_64 | Windows 10 |
| Windows | x86_32 | Windows 10 |
| Linux | x86_64 | - |
| Raspberry Pi OS | armhf | -|
| Android | Arm | Android 4.4 |

<br />　

## 変更点
- SCALING FILTERにxBRZを追加しました。
- Raspberry Pi OS版（XM8_Linux_armhf.deb）を追加しました。
- SDLを2.28.4にしました。
- Androidのビルド環境をSDK 34にしました。
- Androidで画面がブラックアウトしてしまう問題を修正しました。
- ファイルブラウズ関連の実装を見直して、全プラットフォームで文字化けしないようにしました。


<br />　

## 既知の問題
- 特にありません。

<br/>　
<br/>　

# 1.7.3

2023年5月6日 土曜日

<br/>　

## 動作環境
| Platform | CPU | 最小OSバージョン |
| ------------- | ------------------- | ------------- |
| Mac | x86_64 | macOS 10.13 High Sierra |
| Mac | Apple Silicon | macOS 11 Big Sur |
| Windows | x86_64 | Windows 10 |
| Windows | x86_32 | Windows 10 |
| Linux | x86_64 | - |
| Android | Arm | Android 4.1 |

<br />　

## 変更点
- ディレクトリー構成を変更しました。
- Windowsのビルド環境をVisual Studio 2022にしました。
- Androidのビルド環境をSDK 33, Gradle 8, AndroidXなど、できるだけ最新にしました。
- macOS版の最小OSバージョンを10.13に変更しました。
- カーソルキーをテンキーの代わりとして使えるオプションを追加しました。
- 数字キーをテンキーの代わりとして使えるオプションを追加しました。
- d88イメージ読み込み時のチェックを緩和しました。
- Windowsで読み込めないファイル、隠し属性のファイルを表示しないようにしました。

<br />　

## 既知の問題
- Android版で重大な問題があります。

  - バックグラウンドに移行してしまうと、再描画できなくなります。
  - アプリケーションの状態保存が機能しないことがあります。

  

  <br />　

<br/>　


# 1.7.2

2023年4月15日 土曜日

<br/>　

## 動作環境
| CPU           | 最小OSバージョン    |
| ------------- | ------------------- |
| x86_64        | OS X v10.9 Mavericks |
| Apple Silicon | macOS 11 Big Sur    |

<br />　

## 変更点
- x86_64版の最小OSばーじょんを10.7から10.9に変更しました。
- マウスの戻るボタンで、ディレクトリツリーを戻れるようにしました。
- マウスの戻るボタンでも、メニューを戻れるようにしました。
- マウスの移動でソフトキーを表示するか否かを設定できるようにしました。
- マウスのセンターボタンで、ソフトキー表示を消せるようにしました。
- ファイル一覧で、読み込めないファイルを表示しないようにしました。
- ビルドシステムをcmakeに変更しました。

<br />　

## 既知の問題
- なし
<br />　

<br/>　

# 1.7.1

2023年4月2日 日曜日

## 動作環境

| CPU           | 最小OSバージョン    |
| ------------- | ------------------- |
| x86_64        | Mac OS X v10.7 Lion |
| Apple Silicon | macOS 11 Big Sur    |

動作させるにはlibsdlが必要になります。



## 変更点

- 設定で音声の出力先を選べるようにしました。



## 既知の問題

- なし

----

# 1.7.0



## 動作環境

| CPU           | 最小OSバージョン    |
| ------------- | ------------------- |
| x86_64        | Mac OS X v10.7 Lion |
| Apple Silicon | macOS 11 Big Sur    |

動作させるにはlibsdlが必要になります。



## 変更点

- macOSに対応しました。



## 既知の問題

- オーディオ出力の切り替えができません。
