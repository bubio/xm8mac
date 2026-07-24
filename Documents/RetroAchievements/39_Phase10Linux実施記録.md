# Phase 10 Linux移植 実施記録

## 1. 状態

2026-07-24時点で、Linux用RetroAchievementsプラットフォーム層、CMake、配布script、
GitHub Actions、Linux固有自動試験を実装した。Apple Silicon上のUbuntu 22.04 arm64
コンテナでDebug／Release、RA ON／OFF、実HTTPS、ASan／UBSan、DEB生成まで確認した。

現在の判定は **Linux実装・自動受入完了、Linux実機総合受入待ち** とする。SDL画面を使う
実機操作、実Secret Service keyring、proxy／PAC／企業CAはコンテナだけでは確認できない。
また、サニタイザーで判明した共通エミュレーター部を修正したため、Phase 10完了前に
Windows実機回帰も必要である。

## 2. 実装内容

### 2.1 libcurl multi HTTP adapter

- `ra_http_linux.cpp`を追加し、libcurl multi APIを専用worker threadで実行する。
- request追加、個別cancel、全cancel、完了queue、worker shutdownをメインスレッドから分離した。
- APIはredirectを追跡せず、画像だけHTTPS redirectを最大5回追跡する。
- 初期URLとredirect先をHTTPSに限定し、TLS 1.2以上、証明書・hostname検証、system CA、
  libcurl標準proxy設定を使用する。
- GET、空POSTを含むPOST、Content-Type、gzip／deflate等のcontent decoding、connect／total
  timeout、展開後body上限、HTTP status／Content-Type返却を共通契約へ接続した。
- DNS、接続、送受信、TLS接続失敗を再試行可能、timeout、cancel、oversizeをそれぞれ共通の
  transport resultへ分類する。
- request IDの重複と非HTTPS URLを通信開始前に拒否する。

### 2.2 接続状態と資格情報

- `ra_connectivity_linux.cpp`を追加した。外部probeを送信せず、UP状態の非loopback IPv4／IPv6
  interfaceを500 ms cacheで判定する。
- 既存のlibsecret実装をCMakeへ接続した。`libsecret-1`が利用できる場合はSecret Serviceへ
  tokenを保存する。
- `libsecret-1`がない環境でもRA buildを失敗させず、仕様どおりtokenを永続化しない。
  この構成はPkgConfigを無効にした別buildで確認した。

### 2.3 CMake、配布、CI

- Linux RA ON時だけlibcurl、Threads、Linux HTTP／connectivity sourceを追加する。
- DEBへ`libcurl4`、`libsecret-1-0`、RPMへ`libcurl`、`libsecret`の依存関係を追加した。
- Linux x86_64／arm64の通常package workflowとAppImage workflowでRAを有効にし、必要な
  development packageを導入する。
- 通常package workflowでは成果物生成前に全CTestを実行する。
- `dist_app.sh`、`AppImage.sh`、`AppImage_arm64.sh`をRA有効配布buildへ更新した。

### 2.4 Linux adapter試験

`ra_http_linux_test`を追加し、次を確認する。

- 非HTTPS URL拒否
- 不正TLS応答拒否
- 停止TLS接続のtimeout／安全な失敗
- 個別cancelの`Canceled`完了
- active requestを持つclientの即時shutdown
- Linux connectivity monitor生成とpoll
- 環境変数を指定した場合の実HTTPS／system CA

実HTTPS確認では`https://retroachievements.org`までTLS接続し、transport成功とHTTP応答受信を
確認した。サイト側のHTTP 403は未認証の試験clientに対するアプリケーション応答であり、TLS／
CA検証は成功している。

## 3. 自動検証結果

### 3.1 Linux Ubuntu 22.04 arm64

| 構成 | 結果 |
|---|---|
| Debug、RA ON build | 成功 |
| Debug、RA ON CTest | 23/23成功 |
| Debug、RA OFF build／CTest | 成功、8/8成功 |
| Release、RA ON build／CTest | 成功、23/23成功 |
| Release、RA OFF build／CTest | 成功、8/8成功 |
| ASan＋UBSan、RA ON build／CTest | 成功、23/23成功 |
| 実HTTPS／system CA | transport成功、HTTP応答受信 |
| libsecretなしRA platform build | 成功、資格情報非保存message確認 |
| RA OFF binary依存監査 | libcurl、libsecretのlinkなし |
| Release package | 7Z、arm64 DEB生成成功 |
| DEB依存関係 | `libsdl2-2.0-0, libcurl4, libsecret-1-0` |

同一fixture試験により、bank MD5
`5c50ca4f9e3a7afbe4d6666e8974949d`／`ff400f51a2567419b3778691a905952e`、
fixture Game ID `8801001`／`8801002`、schema v1からv2へのmigration、RA stateの媒体・
Game ID判定が既存OSと同じ期待値で成功した。

最初のLinux Release試験では`pathresolver_test`の`realpath`出力bufferがLinuxの`PATH_MAX`より
小さく、Fortifyにより停止した。test bufferを4096 byteへ修正し、再実行で23/23成功した。
Debug全体buildでは`d88write_test`が既存`min` macroと標準headerのinclude順で衝突したため、
標準headerを先に読むよう修正した。

### 3.2 サニタイザーで判明した共通修正

ASan／UBSanでRA以外の既存integration testから次の3件を検出し、意味を変えない最小修正を
行った。

- polymorphicな`DEVICE`を基底pointerから削除していたため、destructorをvirtual化した。
- `I8255` constructorで`rmask`、`mode`、`first`がreset前に未初期化だったため初期化した。
- PSG noise table生成の意図した32 bit wrapを符号付き`int`で行っていたため、`uint`へ変更した。

修正後のASan／UBSan付き全23試験は成功した。

### 3.3 macOS共通回帰

| 構成 | 結果 |
|---|---|
| macOS RA ON build／CTest | 成功、24/24成功 |
| macOS RA OFF Release build／CTest | 成功、10/10成功 |
| workflow YAML読込 | 全8 file成功 |
| whitespace | `git diff --check`成功 |

## 4. Linux実機で必要な受入

次をLinux desktop実機で確認し、結果を本書へ追記する。

1. Normal、Softcore、Hardcoreの起動、logout／login、Offline session、終了。
2. RAメニュー、通知、Achievements、Leaderboards、Library、Load／Save State。
3. keyboard、mouse、controller、画面mode、Full Speed、reset、2 drive、M3U。
4. Secret Serviceが動くdesktop sessionでtoken保存、再起動login、logout削除を確認し、試験用
   credentialを削除する。
5. 利用可能ならsystem proxy、認証proxy、PAC、企業CA、offline／online遷移を確認する。
   該当環境がなければ環境固有の未実施項目として明記する。
6. x86_64 workflow／成果物とAppImageをGitHub Actionsまたはx86_64 Linuxで確認する。
7. 共通サニタイザー修正後のWindows build／CTest／実機基本動作を確認する。

## 5. Phase 10完了条件

Linux実機総合受入と、共通修正後のWindows回帰に重大な問題がなく、結果を本書へ追記した時点で
Phase 10を完了とする。自動試験だけでSDL入力、desktop keyring、環境固有proxyを確認済みとは
扱わない。Phase 10完了まではAndroid Phase 11へ進まない。
