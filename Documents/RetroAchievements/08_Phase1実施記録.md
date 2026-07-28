# Phase 1実施記録: macOS依存物とビルド

## 1. 結果

Phase 1は2026-06-20に完了した。実装はmacOSだけを対象とし、
Windows、Linux、Androidのbuild定義にはRA source、feature flag、stubを追加していない。

| 項目 | 結果 |
|---|---|
| RA既定値 | `XM8_ENABLE_RETROACHIEVEMENTS=OFF` |
| macOS配布script | `-DXM8_ENABLE_RETROACHIEVEMENTS=ON`を明示 |
| RA ON | Debug/Release、Universalでbuild・test成功 |
| RA OFF | Debug/Release、Universalでbuild・既存test成功 |
| 対象architecture | `x86_64 arm64` |
| RA ON test数 | 8 |
| RA OFF test数 | 7 |
| Phase 1開始commit | `cbd792c` |
| 依存物commit | `4dbed52` |
| macOS build統合commit | `5fbbf31` |

文書commitは本ファイルを追加したcommitとする。

## 2. 固定依存物

通常build中のnetwork取得は行わず、次を`ThirdParty/`へsource同梱した。
正規の値は`ThirdParty/versions.json`を参照する。

| 依存物 | 固定値 | 取得物checksum |
|---|---|---|
| rcheevos | v12.3.0 / `e9ca3694c862b61235595176dac4b22677848c93` | archive SHA-256 `75ebe331c5ae0f80d736b11e76a46ad6c59e942c2c9a8c4f41236f3b41a741a5` |
| SQLite | 3.53.0 / source ID `4525003a53a7fc63ca75c59b22c79608659ca12f0131f52c18637f829977f20b` | archive SHA-256 `bf3733d7c71b3ab0f6fd8a9ea0052ad87fa037d94333e14ce09878ba3492c3b0` |
| stb_image | 2.30 / `31c1ad37456438565541f4919958214b6e762fb4` | file SHA-256 `594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3` |

`rcheevos`は固定commitの`Package.swift`に従ってsourceを明示列挙した。
`rc_libretro.c`と`rc_client_external.c`はsource snapshotには保持するがcompileしない。
`RC_DISABLE_LUA`と`RC_CLIENT_SUPPORTS_HASH`をRA targetへ定義する。

SQLiteは`THREADSAFE=1`、default foreign keys、load extension無効、`DQS=0`でcompileする。
`stb_image`はPNG/JPEGのmemory入力だけを有効にし、最大寸法を2048に固定する。

## 3. Build構成

`ThirdParty/CMakeLists.txt`へ次を追加した。

- `xm8_rcheevos`、`xm8_sqlite`、`xm8_stb`: 固定依存物のstatic library。
- `xm8_ra_dependencies`: RA依存物をまとめるinterface target。
- `xm8_ra_core`: `Source/RA/`を再利用するobject library。
- `ra_dependency_test`: 固定versionとcompile設定を検査するRA ON専用test。

RA OFFでは`ThirdParty/`を`add_subdirectory()`せず、`Source/RA/`、rcheevos、SQLite、
stb_imageをcompile・linkしない。RA ONの`.app`だけが`THIRD_PARTY_NOTICES.md`と
5つのライセンス・notice本文を`Contents/Resources`へ収録する。

## 4. 再現command

SDL sourceはPhase 0で取得済みのtreeを指定した。`TYPE`は`Debug`または`Release`、
`RA`は`ON`または`OFF`へ置換する。

```sh
cmake -S . -B /tmp/xm8-phase1-TYPE-RA \
  -DCMAKE_BUILD_TYPE=TYPE \
  -DXM8_ENABLE_RETROACHIEVEMENTS=RA \
  -DCMAKE_OSX_ARCHITECTURES='x86_64;arm64' \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$PWD/build/_deps/sdl2-src"
cmake --build /tmp/xm8-phase1-TYPE-RA -j 4
ctest --test-dir /tmp/xm8-phase1-TYPE-RA --output-on-failure
lipo -archs /tmp/xm8-phase1-TYPE-RA/xm8.app/Contents/MacOS/xm8
```

既定OFFは`-DXM8_ENABLE_RETROACHIEVEMENTS`を渡さない別configureで
`XM8_ENABLE_RETROACHIEVEMENTS:BOOL=OFF`となり、build treeに`ThirdParty/`が
生成されないことを確認した。

## 5. Build・test matrix

| Build type | RA | App architecture | Test architecture | 結果 |
|---|---|---|---|---|
| Debug | ON | `x86_64 arm64` | `x86_64 arm64` | build成功、8/8成功 |
| Debug | OFF | `x86_64 arm64` | `x86_64 arm64` | build成功、7/7成功 |
| Release | ON | `x86_64 arm64` | `x86_64 arm64` | build成功、8/8成功 |
| Release | OFF | `x86_64 arm64` | `x86_64 arm64` | build成功、7/7成功 |

`ra_dependency_test`で次を確認した。

- `rc_version()==12003000`
- `rc_version_string()=="12.3"`
- SQLite version、source ID、threadsafe、固定compile option
- `stb_image`による完全な1x1 PNGのmemory probe

## 6. RA OFF比較

- RA OFFの7つの既存CTestはDebug/Releaseの双方で全件成功した。
- RA OFF build treeに`Source/RA`、`ThirdParty/rcheevos`、`ThirdParty/sqlite`、
  `ThirdParty/stb`、`xm8_ra_*`へのbuild参照がないことを確認した。
- RA OFF実行ファイルに`rc_*`、`sqlite3_*`、`stbi_*` symbolがないことを確認した。
- Phase 0 DebugとPhase 1 RA OFFの`otool -L`を比較し、Foundation、SDL2、libc++、
  libSystem、CoreFoundation、libobjcという動的依存関係が同一であることを確認した。
- RA OFF `.app`のResourceは従来の`AppIcon.icns`だけであり、RA noticeを追加しない。

Phase 1は接続用RA objectをappへlinkするだけで、RA mode、保存形式、D88処理、
VM、メニュー、stateの動作をまだ変更しない。

## 7. ライセンス監査

追加依存物はrcheevosがMIT、SQLiteがPublic Domain、stbがMITまたはPublic Domainである。
取得元、固定version、checksum、本文をsource treeとRA ON app bundleへ収録した。

既存repositoryにはrootのGPLv2本文と、xBRZ sourceおよび`License.txt`のGPLv3表記が
同時に存在する。この組合せのdistribution-wideなライセンス表記はPhase 1では
法的に確定できない。両方の本文を削除・上書きせず配布物へ含め、
`THIRD_PARTY_NOTICES.md`にも併記した。

これはRA追加で生じた競合ではないが、公開配布前にproject ownerまたは法律専門家が
全体ライセンスを確認すべき既存のrelease riskである。本記録は互換性の法的判断を
行うものではない。

## 8. Phase 2へのgate

Phase 1の技術的完了条件は満たした。Phase 2は`5fbbf31`以降を基準に開始できる。
Phase 2で変更できる範囲は、macOSのRAメモリ読出しとD88 hash検証に必要な共通実装、
および最小のVM接続点だけである。後続OSのRA build定義はまだ変更しない。
