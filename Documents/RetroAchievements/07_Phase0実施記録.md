# Phase 0: macOS基準線 実施記録

## 1. 結果

Phase 0は2026-06-20に完了した。本記録の基準は次のとおりである。

| 項目 | 値 |
|---|---|
| 作業開始commit | `bdfbcd1a40d55e9db4df7720b871bf916dac13f9`（tag `1.7.9`） |
| Phase 0実装commit | `0ba7e1a`（`test: establish RA phase 0 D88 baseline`） |
| branch | `codex/ra-phase0` |
| XM8由来情報 | `Documents/README-BUILD.txt`記載のXM8 1.70（2018-01-23）を履歴上の基準とする。現在のePC-8801MAとの同期は行わない |
| macOS | 26.5.1（Build 25F80）、arm64 |
| Xcode | 26.5（Build 17F42） |
| Apple Clang | 21.0.0 |
| CMake / CTest | 4.3.3 / 4.3.3 |
| SDL | 2.32.10、CMakeでsource build |

RA source、RA feature flag、RA依存物はまだ追加していない。Phase 0で追加した実装物は
テスト専用D88生成器とNormal D88書込み回帰試験だけであり、製品targetの実行経路は
変更していない。

## 2. macOS build基準

### 2.1 Debug arm64

実行した構成:

```sh
cmake -S . -B /tmp/xm8-phase0-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$PWD/build/_deps/sdl2-src" \
  -DBUILD_TESTING=ON -DENABLE_PACKAGING=OFF
cmake --build /tmp/xm8-phase0-debug --parallel 4
ctest --test-dir /tmp/xm8-phase0-debug --output-on-failure
```

結果はbuild成功、7/7 test成功、`xm8 --version`は`XM8 1.7.9`である。

### 2.2 Release arm64

Debugの`CMAKE_BUILD_TYPE`とbuild directoryをそれぞれ`Release`、
`/tmp/xm8-phase0-release`へ変えて実行した。結果はbuild成功、7/7 test成功、
`xm8 --version`は`XM8 1.7.9`である。

`FETCHCONTENT_SOURCE_DIR_SDL2`はネットワークを使わず、既にSHA-256検証済みの
SDL 2.32.10展開物を再利用するための実行環境固有指定である。clean環境では省略し、
`Builder/External/SDL2/CMakeLists.txt`記載のURLとhashで取得する。

### 2.3 Universal Release

既存の配布構成を再buildした。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DMACOSX_STANDALONE_APP_BUNDLE=ON \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build build --parallel 6
ctest --test-dir build --output-on-failure
```

結果:

- `build/xm8.app/Contents/MacOS/xm8`はx86_64/arm64のUniversal binary。
- x86_64 sliceのminimum macOSは10.13、arm64 sliceは11.0。
- 7/7 test成功。
- build warningは既存XM8、fmgen、SDLの警告であり、errorはない。警告をPhase 0で
  一括修正して基準を変えることはしない。

## 3. 自動試験基準

| Test | 基準内容 |
|---|---|
| `clidisk_test` | CLI disk指定、bank接尾辞、M3U先頭2媒体、system/clock option |
| `d88probe_test` | 1 bank、2 bank、不正・欠落D88のprobe |
| `d88fixture_test` | 決定論的fixture、bank数、M3U相対path、再生成一致 |
| `d88write_test` | 現行`DISK`によるsector変更、close書戻し、再open後の永続化、size維持 |
| `pathresolver_test` | 共通path解決 |
| `converter_mac_test` | macOS文字列変換 |
| `pathresolver_mac_test` | macOS symlink/alias解決 |

Debug、Release、Universal Releaseのすべてで7/7成功した。さらにReleaseの
`d88write_test`と`d88fixture_test`を個別実行し、正常終了を確認した。

## 4. 著作物を含まないfixture

[../../Tests/Fixtures/README.md](../../Tests/Fixtures/README.md)の手順で次を生成する。

```text
single.d88  1 bank / MD5 5c50ca4f9e3a7afbe4d6666e8974949d
second.d88  1 bank / MD5 ff400f51a2567419b3778691a905952e
multi.d88   2 bank / MD5 9be57f249da12241c8785db0b195216b
pair.m3u    single.d88#0 + second.d88#0
```

各bankはコードで構築したD88 headerと1個の256-byte sectorだけを含む。BIOS、既存D88、
ゲームコード、フォント、画像、音声は読み込まない。生成物はtest outputでありrepositoryへ
追加しない。以後のRA hash testはこの固定MD5を基準にできる。

## 5. Normal実動作基準

Release arm64アプリと生成fixtureを使用してSDL画面上で確認した。

| 操作 | 結果 |
|---|---|
| D88起動 | `multi.d88`をCLI指定し、Drive 1に`XM8 FIXTURE A`と表示 |
| D88書込み | `d88write_test`を手動実行し、sector変更がclose/reopen後も保持され、960 byteのsizeが不変 |
| multi-bank | SDLメニューでDrive 1のbank 1を選び、表示が`XM8 FIXTURE B`へ変更 |
| M3U | `pair.m3u`をCLI指定し、Drive 1=`XM8 FIXTURE A`、Drive 2=`XM8 FIXTURE B`と表示 |
| state save | SDLメニューから未使用slot 9へ保存し、11,842,649 byteのstate生成を確認 |
| state load | 同じslot 9を読み込み、bank Bと画面状態が復元 |
| Full Speed | Option+F11でstatusが`NOWAIT`へ変化し、再操作で通常の約55.4 fps表示へ復帰 |

state試験前に既存`setting.bin`を退避した。試験後はslot 9を削除し、`setting.bin`を復元して
試験前後のSHA-256一致を確認した。fixture以外のD88は変更していない。

## 6. 後続OSの既存Normal build経路

Phase 0では構成調査だけを行い、後続OSのproject、source列挙、依存物を変更していない。

### 6.1 Windows

- 入口: `Builder/Windows/XM8.sln`、`XM8.vcxproj`、Visual Studio 2022 / v143。
- SDL準備: `Builder/Windows/setup_sdl2.ps1`。
- README上の配布対象: Win32、x64、ARM64。Debug/Release構成がある。
- sourceは`XM8.vcxproj`へ個別列挙する方式。
- 現状のDebug ARM64構成はSDLの`lib/x86`を参照しており、Release ARM64の
  `lib/arm64`参照と不一致である。Phase 0では修正せず、Phase 9開始時のNormal build
  事前課題として扱う。
- ARM構成もproject内に残るがREADMEの配布対象ではない。

### 6.2 Linux

- 入口: repository root `CMakeLists.txt`。
- 通常build: `cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo`、
  `cmake --build build`、`ctest --test-dir build`。
- package: `Builder/Linux/dist_app.sh`でdeb/rpm、`AppImage.sh`と
  `AppImage_arm64.sh`でAppImage。
- system SDL2を`find_package(SDL2 REQUIRED)`で使用する。
- Phase 0のmacOSホストではLinux binaryをbuildせず、定義とsource列挙だけを確認した。

### 6.3 Android

- 入口: `Builder/Android`のGradle Wrapper / Android Studio。
- active native buildは`app/build.gradle`から`jni/Android.mk`を呼ぶndk-build。
- `app/CMakeLists.txt`は存在するが、現在のGradle active pathではない。
- commandは`./gradlew assembleDebug`、`assembleRelease`、`lintDebug`。
- JDK 17、AGP 9.2.1、Gradle 9.4.1、NDK 23.2.8568313、minSdk 19、
  compileSdk 36、targetSdk 35。
- ABIはarmeabi-v7a、arm64-v8a、x86、x86_64。
- native sourceは`jni/src/Android.mk`のwildcard列挙。Phase 0ではGradle/Android.mkを
  変更せず、Android buildも開始していない。

## 7. RA実装で予定する既存接続点

コア変更を次へ限定する。詳細契約は01、04、05の各仕様を優先する。

| 接続点 | Phase 1以降の用途 |
|---|---|
| `Source/ePC-8801MA/vm/pc8801/pc88.*` | Main RAM/Text VRAMの副作用なしread API |
| `Source/ePC-8801MA/vm/event.cpp` | 実際の1 frame完了通知 |
| `Source/ePC-8801MA/emu.h`、`Source/UI/emu_sdl.*` | 汎用host callbackの橋渡し |
| `Source/UI/app.*` | session lifecycle、disk起動調停、state、speed、background |
| `Source/UI/video.*` | SDL final present前のoverlay合成とredraw要求 |
| `Source/UI/diskmgr.*` | 作業コピーを通常D88として挿入。RA判定は持ち込まない |
| root CMake / platform build定義 | feature flagと条件付きsource/dependency |

既存`setting.bin`、Normal state format、D88 coreへRA固有データ構造を追加しない。

## 8. Phase 0ゲート判定

| 完了条件 | 証拠 | 判定 |
|---|---|---:|
| macOS既存test成功 | Debug/Release/Universal Releaseで7/7 | 完了 |
| Normal比較項目を記録 | 本書2、3、5節 | 完了 |
| build commandと結果を記録 | 本書2節 | 完了 |
| 著作物なしfixture | generator source、固定hash、`d88fixture_test` | 完了 |
| 後続OSのNormal build経路 | 本書6節 | 完了 |
| 後続OSへRA実装を加えない | git diffでWindows/Linux/Android定義に変更なし | 完了 |

Phase 0のfixture/test実装は`0ba7e1a`へ独立commit済みである。Phase 1は本記録を含む
仕様文書commitを確定した後に開始できる。
