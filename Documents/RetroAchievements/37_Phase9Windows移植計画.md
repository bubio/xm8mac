# Phase 9 Windows移植計画

## 1. 目的と位置付け

Phase 9では、Phase 8までにmacOSで確定したRetroAchievements（以下RA）の共通仕様を
変更せず、Windowsへ移植して受入を完了する。

本書は2026-07-21時点の静的監査結果を基に、Windows実機でVisual Studio buildを始める前に
必要な実装、build matrix、試験、完了条件を固定する。規範となる共通仕様は
[01_要件とアーキテクチャ.md](01_要件とアーキテクチャ.md)、
[02_ゲームライブラリとD88保存仕様.md](02_ゲームライブラリとD88保存仕様.md)、
[03_オーバーレイUI仕様.md](03_オーバーレイUI仕様.md)、
[04_Hardcoreと状態管理.md](04_Hardcoreと状態管理.md)、
[05_実装工程とテスト計画.md](05_実装工程とテスト計画.md)、
[06_外部仕様と依存関係.md](06_外部仕様と依存関係.md)とする。

Phase 8のmacOS受入は完了済みであるため、Phase 9の開始条件は満たしている。Phase 9と
macOS再試験が完了するまでLinux実装へ進まない。

## 2. 固定方針

- macOSで確定したDB schema、D88 hash、state形式、RA session状態機械、overlay挙動、
  Softcore／Hardcore制約をWindows都合で変更しない。
- Windows固有差は`Source/RA`のplatform adapterとbuild定義で吸収する。
- RA無効buildではRA source、rcheevos、SQLite、stb、WinHTTPをcompile／linkしない。
- Windows内でもpathの公開表現はUTF-8とし、Win32 API境界だけUTF-16へ変換する。
- 元D88を変更せず、RAモードではアプリ領域の`working.d88`だけをVMへ渡す。
- Windows固有実装と、既対応OSへ影響する共通コード変更を別commitにする。
- 実機確認で仕様差が見つかった場合は、コード側で暗黙に仕様を変えず、先に本書または
  規範文書へ差異と判断を記録する。

## 3. 事前静的監査結果

2026-07-21の監査時点では、Windows Normal projectは存在するがRA ON構成は未実装である。
macOSの既存`build-ra`ではRA ONの全23 testが成功しており、共通RA機能の基準線自体に
既知の失敗はない。

| 優先度 | 項目 | 現状 | Phase 9での扱い |
|---|---|---|---|
| 必須 | Visual Studio RA構成 | `.vcxproj`／`.filters`にRA、依存物、feature flagがない | RA ON/OFFを条件付きで明示追加 |
| 必須 | Windows HTTP | platform factoryが`nullptr`を返す | WinHTTP adapterを新規実装 |
| 必須 | Windows接続監視 | monitor factoryが`nullptr`を返す | Windows reachability adapterを追加 |
| 必須 | 媒体・DB directory操作 | `dirent.h`、`unistd.h`、`lstat`、POSIX `mkdir`へ無条件依存 | UTF-8共通file adapterへ置換 |
| 必須 | UTF-8 path | narrow CRT file APIが設定path・媒体pathに残る | Win32 wide APIへ集約 |
| 必須 | 置換rename | 画像cache更新がPOSIXの置換renameを前提とする | Windowsのatomic replaceを実装 |
| 必須 | ARM64 Debug | x86 SDL library／DLLを参照する | `lib/arm64`へ修正 |
| 整理 | ARM project構成 | solution対象外のARMが`.vcxproj`に残りx64 SDLを参照する | 対象外構成を削除 |
| 必須 | Windows RA tests | Visual Studio／CTestの実行導線がない | Windows test buildとCTest導線を追加 |
| 必須 | 配布notice | post-buildはSDL2.dllだけをコピーする | RA ON時だけnotice／licenseをコピー |

Credential ManagerのGeneric Credentialによるtoken保存、読込、削除は共通source内に実装済みで
ある。ただしWindows projectへ未登録であり、現在の自動testはmemory backendを使うため、
実Credential Managerの統合確認は未実施である。

SDL overlay描画、keyboard、SDL text input、mouse、controller、frame callback、memory map、
Casual state、Hardcore制約は共通経路へ接続済みである。Windowsでは新しいUIを作らず、既存の
共通接続をbuildと実機試験で検証する。

## 4. 実装計画

### 4.1 UTF-8 file／path操作の共通化

`Source/RA`にRA専用のfile adapterを置き、呼出し側へWin32 APIやPOSIX APIを拡散させない。
adapterの公開引数はUTF-8 `std::string`とし、次の操作を提供する。

- directory tree作成
- file／directory／symlinkまたはreparse pointの種別確認
- recursive directory列挙
- binary read／write
- file削除
- destination置換を含むrename／move
- 親directory取得とpath join

Windows実装は厳密なUTF-8検証後に`MultiByteToWideChar(CP_UTF8,
MB_ERR_INVALID_CHARS, ...)`でUTF-16へ変換し、`CreateDirectoryW`、`GetFileAttributesW`、
`FindFirstFileW`／`FindNextFileW`、`CreateFileW`、`DeleteFileW`、`MoveFileExW`または
`ReplaceFileW`を使用する。

recursive importではjunction／symbolic linkなどのreparse pointを追跡しない。ファイル数上限、
拡張子判定、決定的なsort順は現行仕様を維持する。Windows drive root、UNC path、末尾separator、
日本語を含むpathを個別testで固定する。

置換renameは一時ファイルを同一directoryへ作成し、成功時だけdestinationへ反映する。
失敗時は既存destinationを保持し、一時ファイルを回収する。画像cache、state、媒体作業コピー、
破損DB隔離、credential username hintが同じadapterを利用する。

Windowsで独自にACLを緩和しない。`SDL_GetPrefPath()`が返すユーザー別設定directory配下へ
`ra/`を作り、RAデータを共有directoryや実行ファイルdirectoryへfallbackしない。

この変更はmacOS／将来のLinuxにも影響する共通コード変更として独立commitにし、変更直後に
macOS RA ON 23 test、RA OFF test、sanitizerを再実行する。

### 4.2 WinHTTP adapter

`Source/RA/ra_http_win.h/.cpp`を追加し、共通`RaHttpClient`契約を実装する。

- application単位のWinHTTP sessionを1つ所有する。
- system proxy／automatic proxyを使用し、独自proxy設定を追加しない。
- TLSと証明書／hostname検証はWinHTTP既定を維持し、無効化optionを設定しない。
- `has_post_data == false`をGET、`true`をPOSTとし、空POSTをGETへ変換しない。
- User-Agent、Content-Type、connect timeout、total timeout、response上限を共通requestから反映する。
- gzip／deflateはWinHTTPで展開し、展開後bodyにサイズ上限を適用する。
- API redirectは拒否し、画像GETだけHTTPSからHTTPSへ最大5回追跡する。
- HTTP status、Content-Type、body、transport result、sanitize済みerrorだけを完了queueへコピーする。
- callback contextにはrequest IDだけを保持し、`App`、`RaService`、UIのraw pointerを保持しない。
- `Cancel`は対象requestだけ、`CancelAll`は全requestを取消し、完了は重複配送しない。
- destructorはsession／request handleを閉じ、進行中callbackの終了を待ってから内部stateを破棄する。

platform factoryは`_WIN32`でこのadapterを返す。`winhttp.lib`はRA ON時だけlinkする。

### 4.3 Windows接続監視

通信要求がない期間の切断・復旧もmacOSと同様に表示できるWindows monitorを追加する。
Windows標準のnetwork connectivity情報を使用し、外部URLへの独自probeは行わない。

monitorは`Connected`、`Disconnected`、`Unknown`だけを共通`RaConnectivityState`へ返し、
main threadからの`Poll()`で状態を取得する。COMまたはnotification APIを使用する場合は、初期化した
threadと解放順序を明示し、callbackからUIやRA serviceを直接呼ばない。情報取得に失敗した場合は
誤って切断扱いにせず`Unknown`とする。

### 4.4 Credential Manager

既存の`CredWriteW`、`CredReadW`、`CredDeleteW`実装を維持し、次を補う。

- targetを`net.retropc.pi.XM8.RetroAchievements:<username>`のまま固定する。
- usernameとtargetのUTF-8→UTF-16変換失敗を明示的に拒否する。
- `credentials_user.txt`のI/Oと削除を4.1のfile adapterへ移す。
- 保存token loginが拒否された場合、同じtokenを再試行せずCredential Managerから削除する既存契約を
  Windows実機で確認する。
- token、password、POST body、credential targetをログへ出さない。

`advapi32.lib`の依存をproject上で明示し、memory backendを使うunit testと、実Credential Managerを
使う手動統合試験を分ける。手動試験で作成したcredentialは試験終了時に削除する。

### 4.5 Visual Studio project

`Builder/Windows/XM8.vcxproj`と`.filters`へ次を明示追加する。

- 共通RA C++ source／header
- Windows HTTP／connectivity／file adapter
- rcheevos v12.3.0の必要C source／header
- SQLite amalgamation
- stb image wrapperとheader
- `Source/RA`、rcheevos、SQLite、stbのinclude directory
- rcheevos、SQLite、stbの既存compile definition

project property `XM8_ENABLE_RETROACHIEVEMENTS`を設け、既定`false`とする。`true`の場合だけ
`XM8_ENABLE_RETROACHIEVEMENTS=1`、RA source、third-party source、`winhttp.lib`、
`advapi32.lib`、RA配布物を有効にする。Debug／ReleaseとRA ON／OFFを独立に組み合わせられる
状態にし、例えば次の形で必要時にcommand lineから再現できるようにする。

```powershell
msbuild XM8.sln /m /p:Configuration=Debug /p:Platform=x64 /p:XM8_ENABLE_RETROACHIEVEMENTS=false
msbuild XM8.sln /m /p:Configuration=Debug /p:Platform=x64 /p:XM8_ENABLE_RETROACHIEVEMENTS=true
```

Win32、x64、ARM64の全構成へ同じ条件を適用する。Debug ARM64のSDL参照を`lib/arm64`へ修正し、
現行配布対象ではないARM構成は`.vcxproj`から削除する。

### 4.6 Windows test導線

Windows上で既存RA unit／integration testを実行できるbuild入口を追加する。アプリ本体の正式buildは
明示source列挙の`.vcxproj`を正とし、testにはCMakeのVisual Studio generatorとCTestを使用してよい。
その場合もWindows platform sourceを明示選択し、アプリ本体とのsource／definition parityを検査する。

既存testのPOSIX前提をfile adapterへ移し、少なくとも次を自動化する。

- 共通RA 23 test相当
- file adapterのASCII／日本語／space／長いpath、drive root、UNC形式
- recursive importでreparse pointを追跡しないこと
- destination存在時のatomic replaceと失敗rollback
- WinHTTPのGET、空POST、POST、status、Content-Type、gzip、response上限
- API redirect拒否と画像HTTPS redirect上限
- cancel、CancelAll、timeout、shutdown、callback重複防止
- request ID以外のservice pointerをcallback contextへ保持しないこと
- Credential Manager memory backendと文字変換異常系
- macOSと同じfixtureによるmedia MD5、bank hash、DB schema、state判定

実HTTPS testはRAの本番credentialをsource、test log、CI secretへ保存しない。自動化できない
Credential Manager、proxy、切断復旧、実RA loginは手動受入項目として記録する。

### 4.7 配布物と文書

RA ON buildの出力へ次をコピーする。

- `XM8.exe`
- platformに一致する`SDL2.dll`
- `ThirdParty/THIRD_PARTY_NOTICES.md`
- rcheevos、SQLite、stb、XM8、xBRZのlicense file

RA OFF buildへRA専用依存物を混入させない。READMEのWindows build手順、RA ON/OFF指定、
必要配布物、出力先を更新する。古い`Documents/README-BUILD.txt`は履歴資料として扱い、現行手順の
正として参照させない。

## 5. 実装順序とcommit境界

1. 本計画書を確定する。
2. UTF-8 file adapterとPOSIX依存解消を共通変更commitとして実装し、macOSを再試験する。
3. Windows file、WinHTTP、connectivity、credential接続をWindows固有commitとして実装する。
4. Visual Studio RA ON/OFF構成、ARM64修正、配布物をbuild commitとして実装する。
5. Windows test buildとplatform testをtest commitとして追加する。
6. Windows実機でbuild matrixと自動testを実行し、判明したWindows固有不具合だけを修正する。
7. Windows手動受入後、macOS RA ON/OFF、sanitizer、実RA主要シナリオを再試験する。
8. Phase 9実施記録へcommit、command、結果、未実施項目、承認済み差異を記録する。

共通コード変更とWindows固有変更を同一commitへ混在させない。実Windows buildでしか判明しない
修正が共通コードへ及ぶ場合も、別commitに分けてmacOS再試験を添える。

## 6. Windows手動受入build matrix

Visual Studio 2022、toolset v143、Windows 10 SDKを基準とし、
`Builder/Windows/setup_sdl2.ps1`で固定SDL2を準備する。

このmatrixは移植時の手動受入用であり、CI jobや配布物をRA ON／OFFの2系列へ増やすものではない。
通常CIと配布物はRA ONのWin32、x64、ARM64各1種類を維持し、RA OFFは必要時の互換確認に限る。

| Platform | Configuration | RA OFF | RA ON |
|---|---|---:|---:|
| Win32 | Debug | 必須 | 必須 |
| Win32 | Release | 必須 | 必須 |
| x64 | Debug | 必須 | 必須 |
| x64 | Release | 必須 | 必須 |
| ARM64 | Debug | 必須 | 必須 |
| ARM64 | Release | 必須 | 必須 |

全12 buildでwarningとlink依存を記録する。x64実機でRA ON／OFFの動作受入を行う。Win32は同一実機で
実行可能な範囲を確認する。ARM64はbuildを必須とし、ARM64実機を利用できない場合はruntime未確認を
明記してPhase 9完了判定時に扱いを決め、暗黙に実行確認済みとはしない。

## 7. 自動試験

### 7.1 RA OFF／Normal

- RA source、rcheevos、SQLite、stb、WinHTTPがcompile／linkされていないこと。
- 既存unit testが成功すること。
- `XM8.exe`のimport／dependencyを検査し、RA専用依存がないこと。

### 7.2 RA ON

- 4.6のWindows対応testを全件実行する。
- 同一fixtureからmacOSと同じmedia MD5、bank別RA hash、DB schema version、state判定を得る。
- original D88の試験前後SHA-256が一致する。
- test一時directoryとCredential Manager entryを終了時に回収する。

### 7.3 HTTP backend共通適合

- GET、空POST、通常POSTのmethodとbodyが保存される。
- system proxy経由で通信できる。
- TLS／証明書errorを成功扱いしない。
- gzip展開後のbody上限が働く。
- API redirectを追跡しない。
- 画像はHTTPS redirectだけ最大5回追跡する。
- cancel、timeout、DNS失敗、接続失敗、oversizeを共通transport resultへ分類する。
- shutdown後にcallback、handle、threadが残らない。

## 8. Windows手動受入

### 8.1 Normal回帰

1. RA OFF Release x64を起動する。
2. single-bank D88、multi-bank D88、2枚のD88、M3Uを順に開く。
3. D88書込み、bank切替、Drive 1／2交換を確認する。
4. Normal stateの保存と復元を確認する。
5. Alt+F11でFull Speedと通常速度を切り替える。
6. keyboard、mouse、controller、drag and dropを確認する。
7. 終了後、設定とD88の期待した変更以外にRA directoryが作られていないことを確認する。

### 8.2 RA基本機能

1. RA ON Release x64を新規設定で起動し、RA既定OFFを確認する。
2. RA modeをONにし、初回play modeがHardcoreであることを確認する。
3. username／password loginを行い、再起動後に保存token loginが成功することを確認する。
4. Credential ManagerにGeneric Credentialが1件だけ存在し、tokenが通常ファイルへ保存されて
   いないことを確認する。
5. D88、M3U、folder importを行い、原本ではなく`working.d88`がVMへ渡ることを確認する。
6. Library、Game Detail、Achievements、Leaderboards、Rich Presence、badge／avatarを確認する。
7. keyboard、SDL text input、mouse、controllerでoverlayを操作する。
8. 日本語Windows accountまたは日本語を含む試験pathで設定保存、媒体import、state、画像cacheを
   確認する。
9. logout後にCredential Manager entryとusername hintが削除されることを確認する。

### 8.3 Offline／接続復旧

1. Active session中にnetworkを切断し、resetなしで`disconnected`表示へ遷移することを確認する。
2. networkを復旧し、`reconnected`表示へ遷移することを確認する。
3. 起動時の認証またはゲームloadを失敗させ、Offline sessionとしてゲームが継続することを確認する。
4. Offline session途中でnetworkを戻してもRA評価を再開しないことを確認する。
5. proxy環境を利用できる場合はWindows system proxyが使われることを確認する。

### 8.4 Casual state

- CasualでRA専用stateを保存し、同じゲーム／mode／媒体で復元できる。
- 別ゲーム、別mode、別媒体、破損chunk、旧Normal stateを拒否する。
- 保存／読込失敗で既存stateとlive sessionを破損しない。

### 8.5 Hardcore

- Active Hardcore中にLoad／Save、Full Speed、Pseudo fast diskをmenu、shortcut、CLIから迂回できない。
- Hardcore中のXM8 menu、Library、Achievements、Leaderboards表示中もVM frameとRA評価が継続する。
- Reset要求を次のVM frameより前に処理する。
- 切断だけではActive Hardcore制約を弱めない。
- RA開始失敗後のOffline sessionではHardcore表示と制限を残さない。
- Casualへの切替確認、cold reset、再login後のmode永続化がmacOSと一致する。

## 9. macOS再試験

Windows移植で共通コードを変更した後、macOSで次を再実行する。

- RA OFF Debug／Release buildと全Normal test
- RA ON Debug／Release buildと全RA test
- ASan／UBSan RA ON test
- fixtureのmedia MD5、bank hash、DB schema、state判定
- 実RA login、ゲーム識別、画像、Casual state、Hardcore禁止操作の主要シナリオ
- original D88の試験前後SHA-256一致

macOSで回帰した共通変更をWindows都合として残さない。

## 10. 完了条件

Phase 9は次をすべて満たした場合だけ完了とする。

- Win32、x64、ARM64のDebug／Release、RA ON／OFF全12 buildが成功する。
- Windows RA ONで共通RA unit／integration testとWindows platform testが成功する。
- Windows RA OFFで既存Normal buildと回帰試験が成功し、RA依存を含まない。
- WinHTTPのfake、実HTTPS、proxy、cancel、timeout、shutdown適合を確認している。
- Credential Managerの保存、保存token login、拒否時削除、logout削除を確認している。
- UTF-8 path、recursive import、atomic replaceがWindowsで成功する。
- SDL overlayとkeyboard、text input、mouse、controllerが動作する。
- Casual stateとHardcoreの共通受入シナリオが成功する。
- macOSと同じfixtureから同じMD5、RA Game ID、DB schema、state判定を得る。
- 原本D88の試験前後hashが一致する。
- RA ON配布物にnotice／licenseが含まれ、RA OFF配布物へ混入しない。
- 共通コード変更後のmacOS再試験が成功する。
- build command、test結果、実機環境、未実施項目、承認済み差異をPhase 9実施記録へ残している。

いずれかを未確認のまま「Windows対応完了」と記録しない。ARM64など実機を用意できない項目は
build成功とruntime受入を分けて記録し、完了判定時に明示的に扱う。

## 11. 主なリスクと対策

| リスク | 対策 |
|---|---|
| WinHTTP callbackとshutdownの競合 | request ID管理、完了queue、callback数の待機、破棄順序test |
| narrow path混入 | RA file操作をadapterへ集約し、日本語path testを必須化 |
| Windows renameの非置換動作 | `ReplaceFileW`／`MoveFileExW`でrollback可能な置換を実装 |
| junctionによるrecursive loop | reparse pointを追跡せず候補数上限も維持 |
| RA ON/OFF source差の崩れ | project propertyの条件を一箇所へ集約し、binary dependencyも検査 |
| Win32／ARM64だけの型・link問題 | 全platformのDebug／Releaseを実Windows build matrixへ含める |
| Windows移植によるmacOS回帰 | 共通変更をcommit分離し、変更ごとにmacOS testを再実行 |
| tokenやpathのlog漏洩 | HTTP／credential errorを固定分類し、OS error文字列へ秘密値を連結しない |

## 12. Windows実機作業へのhandoff

Windows実機で最初に行う作業はRA実装buildではなく、未変更のNormal projectの基準線確認とする。

1. Visual Studio 2022、v143、Windows SDK、PowerShell、CMakeのversionを記録する。
2. `setup_sdl2.ps1`を実行し、x86／x64／ARM64の配置とfile versionを記録する。
3. 未変更基準線のWin32／x64／ARM64 Debug／Release build結果を記録する。
4. Debug ARM64の既知SDL参照不整合を再確認する。
5. Normal x64の起動、D88、M3U、state、Full Speedを確認する。
6. その結果を基準に、本書4章の実装を順に適用する。

Windows実機で初めて判明した問題は、再現条件、対象platform／configuration、error全文、
RA ON／OFF差、最小修正範囲を記録してから修正する。
