# XM8 Android Build

このディレクトリがAndroid版のビルド入口です。リポジトリルートではなく、
`Builder/Android` をAndroid Studioで開くか、このディレクトリのGradle Wrapperを
使用してください。

## 対応範囲

- 最低動作環境: Android 4.4 / API 19
- compile SDK: Android 16 / API 36
- target SDK: API 35
- ABI: `armeabi-v7a`, `arm64-v8a`, `x86`, `x86_64`
- SDL: 2.32.10

Android 17 / API 37は、2026年6月14日時点ではPlatform Stability到達前のBetaです。
正式版になるまでは互換テストにのみ使用し、compile SDKには採用しません。

## 必要な環境

- JDK 17
- Android SDK Platform 36
- Android SDK Build-Tools 36.0.0
- Android NDK 23.2.8568313

GradleとAndroid Gradle PluginはWrapperから取得されます。

- Gradle 9.4.1
- Android Gradle Plugin 9.2.1

NDK r28以降は最低対応APIが21であるため、Android 4.4対応を維持するこのプロジェクト
では使用できません。NDKは必ず23.2.8568313をインストールしてください。

## SDLの準備

初回ビルド前、またはSDLを入れ直す場合に実行します。

```shell
cd Builder/Android
./setup_sdl2.sh
```

スクリプトは公式SDL 2.32.10を取得し、
`patches/sdl2-2.32.10-android-compat.patch` を適用してからnativeソースとAndroid
Javaソースを所定の場所へ配置します。

`app/src/main/java/org/libsdl/app` は生成先です。互換修正を追加・変更する場合は生成先
だけを直接編集せず、同じ変更を上記パッチへ反映してください。パッチにはAndroid
14以降のReceiver登録、Android 15のEdge-to-Edge、Bluetooth/録音権限、API 19互換
ガードが含まれます。

## コマンドラインビルド

`JAVA_HOME` がJDK 17を指していることを確認してください。

```shell
cd Builder/Android
./gradlew assembleDebug
./gradlew assembleRelease
./gradlew lintDebug
```

debug APKは以下へ生成されます。

```text
app/build/outputs/apk/debug/app-debug.apk
```

署名用の `release.keystore` がこのディレクトリに存在する場合、以下の環境変数を使用
してrelease APKへ署名します。

```text
KEYSTORE_PASSWORD
KEY_ALIAS
KEY_PASSWORD
```

## AARビルド

```shell
cd Builder/Android
./gradlew assembleDebug -PBUILD_AS_LIBRARY
```

以下の2つが生成されます。

```text
app/build/outputs/aar/app-debug.aar
app/build/outputs/aar/net.retropc.pi.aar
```

## 互換性上の注意

- Javaコンパイル互換レベルはJava 8です。JDK 17はGradle/AGPの実行に使用します。
- native側の最低APIも `android-19` に固定しています。
- C++ランタイムはstatic linkし、NDK r23の4KB alignment版
  `libc++_shared.so` をAPKへ同梱しません。
- 64bit nativeライブラリは16KBページサイズ対応のELF alignmentでリンクします。
- API 30以上の没入表示は `WindowInsetsController`、API 19-29は従来のsystem UI
  visibility flagsを使用します。
- `MANAGE_EXTERNAL_STORAGE` と既存のStorage Access Framework動作は維持しています。

## 成果物の確認

16KBページサイズ対応はBuild-Toolsの `zipalign` とNDKの `llvm-readelf` で確認します。

```shell
zipalign -c -P 16 -v 4 app/build/outputs/apk/debug/app-debug.apk
```

4 ABIの各 `.so` について、LOAD segment alignmentが `0x4000` 以上であることも
確認してください。

実行確認では、少なくとも以下を確認します。

- API 19相当での起動
- API 35/36でのEdge-to-Edgeと没入表示
- Display Cutout
- ジェスチャーナビゲーションと3ボタンナビゲーション
- ROM読込、Intent起動、SAF
- 音声、タッチ、戻る操作、物理キーボード、USB HID
