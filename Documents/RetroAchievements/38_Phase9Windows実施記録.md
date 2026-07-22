# Phase 9 Windows移植 実施記録

## 1. 状態

2026-07-22時点で、Windows移植コード、Visual Studio RA ON／OFF構成、Windowsで必要に応じて
利用できるCTest導線まで実装した。既存のWindows CI workflowは変更していない。

現在の判定は**実装完了・Windows実機受入待ち**である。作業環境がmacOSのため、Visual Studio
2022による実build、Windows上のCTest、Credential Manager、proxy、切断復旧、実RA login、
x64手動受入はまだ実行していない。これらを実施するまでPhase 9受入完了とはしない。

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

## 3. macOS回帰結果

Windows向け共通コード変更後、macOSで次を実行した。

| 構成 | command | 結果 |
|---|---|---|
| RA ON build | `cmake --build build-ra -j4` | 成功 |
| RA ON test | `ctest --test-dir build-ra --output-on-failure` | 24/24成功 |
| RA OFF build | `cmake --build build -j4` | 成功 |
| RA OFF test | `ctest --test-dir build --output-on-failure` | 10/10成功 |
| Visual Studio XML | `xmllint --noout` | project／filters／propsすべて成功 |
| whitespace | `git diff --check` | 問題なし |

従来23件だったRA ON testは、file adapter test追加により24件となった。

## 4. Windowsで実行する残作業

1. Visual Studio 2022／v143／Windows 10 SDKで手動受入buildを実行する。RA OFF確認は
   互換性確認用であり、配布物として保存しない。
2. RA OFF binaryにRA source、WinHTTP、SQLite等が混入していないことを確認する。
3. x64 RA ONでCTestを実行し、全RA testの結果を本書へ追記する。
4. WinHTTPのGET、空POST、POST、gzip、redirect、oversize、cancel、timeout、shutdownを
   Windows上のtest serverで確認する。
5. 日本語Windows username、日本語D88 path、space、長いpath、UNC pathを確認する。
6. Credential Managerの保存、再login、拒否token削除、logout削除を確認し、試験credentialを消す。
7. system proxy、offline／online遷移、証明書errorを確認する。
8. Normal回帰、Softcore、Hardcore、overlay、mouse／keyboard／controller、2 drive、M3Uを確認する。
9. ARM64実機がなければruntime未確認を明記する。
10. macOS sanitizerと実RA主要シナリオを再実行する。

## 5. Phase 9完了条件

上記Windows残作業に重大な失敗がなく、結果を本書へ追記した時点でPhase 9を完了とする。
CIの通常buildだけではWindows RA build成功の証跡にしない。Windows受入完了後にmacOS最終回帰を行い、
その完了までLinux Phase 10へ進まない。
