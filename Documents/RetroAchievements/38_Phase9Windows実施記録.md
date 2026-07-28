# Phase 9 Windows移植 実施記録

## 1. 状態

2026-07-22時点で、Windows移植コード、Visual Studio RA ON／OFF構成、Windowsで必要に応じて
利用できるCTest導線まで実装した。既存のWindows CI workflowは構成と3配布物を維持したまま、
通常のRelease buildでRAを有効にした。

Windows CIではx64／Win32／ARM64のRA有効Release buildと3成果物の生成に成功した。また、
Windows実機でRAの基本動作に問題がないことを確認した。詳細な受入項目、Windows上のCTest、
Credential Manager、proxy、切断復旧等は引き続き未確認であるため、現在の判定は
**実装完了・Windows詳細受入継続中**とする。

## 2. 実装内容

### 2.1 UTF-8 file adapter

- `Source/RA/ra_file_util.h/.cpp`を追加した。
- WindowsではUTF-8を厳密にUTF-16へ変換し、wide pathでfile／directory操作する。
- directory tree作成、no-follow種別確認、列挙、read／write、copy、move／replace、再帰削除を
  共通化した。
- reparse point／symbolic linkをdirectoryとして再帰しない。
- library DB、媒体copy、画像cache、state、credential username hint、D88 probeをadapterへ移した。
- Windowsの既存destination置換は`MoveFileExW`の`MOVEFILE_REPLACE_EXISTING`を使用する。
- `m3u.cpp`と`pathresolver.cpp`へWindows UTF-8 path対応を追加した。

### 2.2 Windows platform adapter

- `ra_http_win.cpp`を追加し、WinHTTP非同期clientを実装した。
- system automatic proxy、既定TLS検証、gzip／deflate、timeout、body上限を使用する。
- API redirectは自動追跡せず、画像GETだけHTTPS redirectを最大5回追跡する。
- callback contextは一意なtransport IDだけとし、`App`、`RaService`、UI pointerを保持しない。
- request stateは`WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING`までregistryで保持し、destructorは
  全request handleのclose完了を待ってからsessionを閉じる。
- cancel、timeout、retryable error、oversizeを共通transport resultへ変換する。
- `ra_connectivity_win.cpp`を追加し、外部probeを行わずWindows接続状態を500 ms cacheで取得する。
- Credential Manager実装をWindows projectへ接続し、username hintのfile操作を共通化した。

WinHTTPの寿命管理はMicrosoftの`WinHttpCloseHandle`、status callback、option仕様に従った。

- <https://learn.microsoft.com/windows/win32/api/winhttp/nf-winhttp-winhttpclosehandle>
- <https://learn.microsoft.com/windows/win32/api/winhttp/nc-winhttp-winhttp_status_callback>
- <https://learn.microsoft.com/windows/win32/winhttp/option-flags>

### 2.3 Visual Studio／配布物

- 全platform共通名の`XM8_ENABLE_RETROACHIEVEMENTS` propertyを追加し、既定値を`false`とした。
- `true`のときだけRA source、rcheevos、SQLite、stb、`XM8_ENABLE_RETROACHIEVEMENTS=1`、
  `winhttp.lib`、`wininet.lib`、`advapi32.lib`を有効にする。
- sourceと依存定義は既存の`Builder/Windows/XM8.vcxproj`へ直接追加した。
- RA ON buildではthird-party noticeと5 licenseを出力先へコピーする。
- solution対象外だったARM project構成を削除した。
- Debug ARM64のSDL参照をx86からarm64へ修正した。
- READMEへRA ON／OFF command lineを追加した。

### 2.4 Windows test導線

- Windows workflowは従来のRelease Win32／x64／ARM64 buildと3配布物を維持し、既存の
  3つの`msbuild`へRA有効propertyだけを追加した。
- RA ON/OFF matrixやCTest実行は既存CIへ追加していない。
- CMake Visual Studio generatorを使うローカルCTest導線は、Windows実装確認用として維持する。
- `ra_file_util_test`を追加し、日本語・空白path、binary read／write、copy、replace、列挙、
  size limit、再帰削除を確認する。
- Windows testを妨げていたRA test内のPOSIX directory操作を共通adapterへ移した。
- WindowsではHTTP client生成、非HTTPS拒否、connectivity monitor生成を自動確認する。

### 2.5 共通overlay修正

- Windows実機確認後、200ライン・Scan Line OFFで起動またはresetした状態からRA loginを開くと、
  login画面だけが縦2倍になる問題をmacOSとWindowsの共通描画経路で確認した。
- login画面だけが400ライン座標のまま通常frame bufferへ描かれ、200ライン表示時の拡大を受けていた。
- 他のRA全画面UIと同じ400ライン固定のmenu textureへ描画し、login成功時にmenu modeを解除する
  よう修正した。400ライン表示とScan Line設定に依存せず同じ座標系で表示する。

## 3. macOS回帰結果

Windows向け共通コード変更後、macOSで次を実行した。

| 構成 | command | 結果 |
|---|---|---|
| RA ON build | `cmake --build build-ra -j4` | 成功 |
| RA ON test | `ctest --test-dir build-ra --output-on-failure` | 24/24成功 |
| RA OFF build | `cmake --build build -j4` | 成功 |
| RA OFF test | `ctest --test-dir build --output-on-failure` | 10/10成功 |
| Visual Studio XML | `xmllint --noout` | project／filters成功 |
| whitespace | `git diff --check` | 問題なし |

従来23件だったRA ON testは、file adapter test追加により24件となった。

Windows CI run `29882728877`ではRA有効Releaseのx64／Win32／ARM64がすべて成功し、従来どおり
3成果物を生成した。Windows実機ではRAの基本動作に問題がないことを確認した。

### 3.1 Windows Credential Manager単独試験（2026-07-23）

Windows x64 Releaseで`ra_credentials_win_test`を追加し、実メモリ代替を使わずWindows
Credential Managerへ一意な試験credentialを保存した。`CredReadW`による存在確認、storeからの
再読込、username hint fileへのtoken非混入、削除、削除後の読込失敗を確認した。試験終了後、
試験credentialと一時directoryが残っていないことも確認した。

既存の`ra_credentials_http_test`と`ra_service_test`も同じWindows buildで実行し、保存token login、
拒否token削除、logout削除を含む認証ライフサイクルを確認した。3 testは3/3成功した。

この実機のWinHTTP設定は`Direct access (no proxy server)`であり、ユーザーのsystem proxy、PAC、
WPADも設定されていなかった。個人環境の設定を変更しない方針とし、proxy経由、認証proxy、
不正proxyからの復旧は未実施のままとする。

### 3.2 Windows x64 CMake build／CTest／path／WinHTTP（2026-07-23）

Visual Studio 2026、MSVC 19.51、Windows SDK 10.0.26100.0でCMakeのWindows x64 Releaseを
検証した。CMake targetがVisual Studio projectのUnicode character setと`imm32` linkを再現して
いなかったため、`UNICODE`／`_UNICODE`と`imm32`を追加した。修正後はRA ON／OFFともXM8本体の
buildに成功した。

| 構成 | 結果 |
|---|---|
| Windows x64 RA ON Release build | 成功 |
| Windows x64 RA ON CTest | 24/24成功 |
| Windows x64 RA OFF Release build | 成功 |
| Windows x64 RA OFF CTest | 8/8成功 |

Windows CTestで判明した`/tmp`固定test、SDL2 DLL未配置、`DISK` test objectのstack overflowを
Windows対応した。file adapter testには日本語、space、不正UTF-8に加え、`MAX_PATH`を超える
UTF-8 pathのdirectory作成、書込、読込、再帰削除を追加した。Windows file adapterは長い
drive path／UNC pathをextended-length pathへ変換する。

`ra_http_win_test`ではloopback TCPだけを使用し、不正TLS応答をHTTP成功として扱わないこと、
TLS応答停止時に安全に失敗すること、個別cancelが`Canceled`として完了することを確認した。
system proxyや証明書storeは変更していない。GET／POST／gzip／redirect／oversizeの実TLS server
試験と、`Timeout`分類を確実に発生させる試験は引き続き未実施である。

RA OFF生成projectとbinary importを確認し、RA source、rcheevos、SQLite、WinHTTPのcompile／link
およびimportがないことを確認した。

### 3.3 実機受入・最終回帰・Phase 9完了（2026-07-24）

Windows実機でNormalおよびRAの実働を確認し、問題がないことを確認した。ARM64実機でもruntimeを
確認し、問題がないことを確認した。Windows対応後のmacOS最終動作確認も完了し、問題はなかった。

この個人環境にはproxy、PAC／WPAD、認証proxy、企業証明書環境がないため、それらの環境固有項目は
未実施として記録する。利用可能な環境での通常接続、Windows自動試験、Windows実機受入、ARM64実機、
macOS最終回帰が完了し、重大な失敗がないため、2026-07-24付でPhase 9を完了と判定する。

## 4. Windows作業項目と結果

1. 必要に応じてVisual Studio 2022／v143／Windows 10 SDKでローカル手動buildを実行する。
   RA OFF確認は互換性確認用であり、配布物として保存しない。
   2026-07-23にVisual Studio 2026／MSVC 19.51でRA ON／OFFを確認済み。v143は未確認。
2. RA OFF binaryにRA source、WinHTTP、SQLite等が混入していないことを確認する。
   2026-07-23にCMake生成projectとbinary importで確認済み。
3. x64 RA ONでCTestを実行し、全RA testの結果を本書へ追記する。
   2026-07-23に24/24成功。
4. WinHTTPのGET、空POST、POST、gzip、redirect、oversize、cancel、timeout、shutdownを
   Windows上のtest serverで確認する。
   cancel、不正TLS、停止TLSの安全な失敗は確認済み。実TLS response項目は未確認。
5. 日本語Windows username、日本語D88 path、space、長いpath、UNC pathを確認する。
   日本語、space、`MAX_PATH`超過local pathは自動試験済み。日本語accountと実UNC共有は未確認。
6. Credential Managerの保存、再login、拒否token削除、logout削除を確認し、試験credentialを消す。
   2026-07-23に自動試験で確認済み。
7. system proxy、offline／online遷移、証明書errorを確認する。
   proxy環境はないため環境固有項目は未実施。不正TLSの安全な拒否は自動試験済み。
8. Normal回帰、Softcore、Hardcore、overlay、mouse／keyboard／controller、2 drive、M3Uを確認する。
   login overlayは200ラインのScan Line ON／OFFと400ラインで同じ寸法になることを再確認する。
   2026-07-24にWindows実機の実働に問題がないことを確認済み。
9. ARM64実機がなければruntime未確認を明記する。
   2026-07-24にARM64実機runtimeを確認済み。
10. macOS sanitizerと実RA主要シナリオを再実行する。
   2026-07-24にmacOS最終動作確認済み。

## 5. Phase 9完了条件

上記Windows残作業に重大な失敗がなく、結果を本書へ追記した時点でPhase 9を完了とする。
CIの通常buildだけではWindows RA build成功の証跡にしない。Windows受入完了後にmacOS最終回帰を行い、
その完了までLinux Phase 10へ進まない。

2026-07-24、Windows x64 RA ON／OFF build・CTest、Credential Manager、path、WinHTTP安全失敗、
Windows実機、ARM64実機、macOS最終回帰の結果をもって上記条件を満たした。Phase 9を完了し、
Linux Phase 10へ進む。
