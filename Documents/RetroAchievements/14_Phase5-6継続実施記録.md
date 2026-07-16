# Phase 5-6継続実施記録: 実フレーム評価と現状再監査

## 1. 位置づけ

本記録は、2026-07-15時点の`codex/retroachievements-integration`を再監査し、
[12_Phase5実施記録.md](12_Phase5実施記録.md)以降に追加された実装と、
Phase 5「macOS RAセッションとフレーム評価」の継続作業を記録する。

再監査開始時のHEADは`5cf94c8`である。本記録に記載する実フレームcallback、
追加test、診断toolは、このHEAD以降の作業として扱う。

## 2. 既存記録以降に確認した実装

`12_Phase5実施記録.md`の「未完了項目」のうち、現コードでは次が実装済みである。

- Library一覧、Game Detail、STARTからのworking copy起動。
- 起動profileへのDrive 1、Drive 2媒体保存。
- Achievements一覧の選択、スクロール、詳細画面。
- Leaderboard一覧、scoreboard、上位順位取得と表示。
- ゲームと実績badgeのHTTP取得、RGBA decode、描画。
- Login以外の一覧画面に対するkeyboard、mouse、controller、touch入力。

一方、これらはPhase 6の完全な完了を意味しない。Settings、ASCIIオンスクリーン
キーボード、永続画像cacheとeviction、共通text renderer、toast queue、Challenge/
tracker常駐表示、screenshot比較は引き続き未完了である。

## 3. 実VMフレーム評価の修正

### 3.1 修正前

修正前は、`EVENT::create_sound32()`が複数の`drive()`を完了した後、App側が
返された`extra_frames`回数だけ`ProcessRaService(true)`を呼んでいた。

この処理は`rc_client_do_frame()`の呼出し回数自体は実行frame数と一致するが、
全呼出しが音声buffer生成後の同じVMメモリを読む。したがって、中間frameだけ成立した
AchievementまたはLeaderboard条件を取りこぼす可能性があった。

### 3.2 修正後

- RAに依存しない`HostFrameCallback`をevent managerへ追加した。
- `EVENT::drive()`の1 frame処理とsound mixが完了した直後にcallbackを1回通知する。
- Appはcallbackから`RaService::DoFrame()`だけを同期実行する。
- HTTP完了drain、画像処理、menu更新、SDL通知描画はcallbackから実行しない。
- callbackはVM生成時と`ChangeSystem()`によるVM再生成時の双方で再接続する。
- 音声buffer生成後に`extra_frames`回まとめて評価していた旧loopは削除した。
- 非同期RA処理とevent-to-notice変換はVM lock解除後に1回実行する。

これにより、描画skip、Full Speed、1回の音声buffer生成で複数frameが進む場合も、
各frame直後のメモリを`rc_client_do_frame()`へ渡せる構造になった。

### 3.3 background idle

RA modeでserviceが存在する間は、background中の`SDL_WaitEvent()`を最大1秒の
`SDL_WaitEventTimeout()`へ切り替えた。timeout後にHTTP完了drainと
`rc_client_idle()`を実行する。Normal modeの無期限waitは変更しない。

### 3.4 Rich Presence表示

ASCENDの実機確認で、スコアが10点変化するたびにRich Presenceが更新され、5秒の
toastがほぼ常時表示されることを確認した。単一slotのtoastを次のRich Presenceが
上書きするため、Achievement解除通知を見失う可能性もあった。

- `RichPresenceChanged`をtoast通知へ変換しない。
- `RetroAchievements`メニューへ`Now:`専用行を追加する。
- 行自体は従来のメニュー幅でclipする。
- `Now:`行へfocusした時は、画面下部の詳細欄へ全文を表示する。
- 長文は1秒静止後、既存の一覧詳細欄と同じ文字単位の横scrollで読めるようにする。

Achievement解除、Leaderboard結果、切断などの一時イベントは従来どおりtoastへ
表示する。

利用者によるASCEND実機確認では、Rich Presenceの連続toastが停止し、`Now:`行への
focus時に画面下部で全文を確認できることを確認した。表示上の問題は報告されなかった。

## 4. 自動test

次を追加した。

- `host_frame_callback_test`
  - callback未設定時に呼ばれないこと。
  - 3回のframe通知でcallbackが3回呼ばれること。
  - callback再接続時に古いuserdataを参照しないこと。
  - callback解除後に呼ばれないこと。
- `ra_service_test`のframe境界test
  - frame 1では検査memoryを`0`、frame 2では`1`に変更する。
  - `0xH0000=1`のAchievementがframe 1では解除されず、frame 2で解除されること。
  - 各`DoFrame()`がその時点のemulated memoryを読み直すこと。

実行結果:

```sh
cmake --build build-ra --target xm8 host_frame_callback_test \
  ra_dependency_test ra_media_probe_test ra_memory_map_test \
  ra_library_store_test ra_seed_library_fixture_test \
  ra_credentials_http_test ra_service_test ra_overlay_test
ctest --test-dir build-ra \
  -R 'host_frame_callback_test|ra_|d88fixture_test|d88probe_test|clidisk_test' \
  --output-on-failure
```

- macOS RA ON build: 成功。
- RA ON自動test: 12件中12件成功。
- macOS RA OFF Release build: 成功。
- RA OFF基礎test: 4件中4件成功。
- `git diff --check`: 成功。

## 5. ASCEND D88検証

利用者提供の次のD88をmacOSローカル検証媒体とする。

```text
/Volumes/CrucialX6/roms/PC88/TEST/ascend.d88
```

著作物である可能性があるため、D88本体または内容はrepositoryへ追加しない。
パスはローカル手動試験用であり、自動testは従来どおり生成fixtureを使用する。

`ra_inspect_media`を追加し、`RaMediaProbe`と同じ`rcheevos`経路で次を確認した。

| 項目 | 結果 |
|---|---|
| file size | 348848 bytes |
| D88 bank数 | 1 |
| bank 0名 | `ASCEND` |
| 保存管理用media MD5 | `5def00835e061a54fe6d1fa2d5a8d2b0` |
| bank 0 RA hash | `5def00835e061a54fe6d1fa2d5a8d2b0` |

単一bank D88のため、保存管理用hashとRA識別hashは同一である。

一時RA rootへimportし、次も確認した。

- `game_id=1`のローカルgame行を作成した。
- `ra/media/5def00835e061a54fe6d1fa2d5a8d2b0/working.d88`を生成した。
- launch profileのDrive 1が原本ではなくworking copyを返した。
- import後も原本MD5は不変だった。
- working copyのMD5は原本と一致した。

再現command:

```sh
cmake --build build-ra --target ra_inspect_media
./build-ra/ra_inspect_media \
  --import-root /tmp/xm8-ra-ascend-20260715 \
  /Volumes/CrucialX6/roms/PC88/TEST/ascend.d88
```

実RA Game ID、タイトル、実績定義の取得とゲーム実行中の解除確認は、実HTTPS、
利用者アカウント、外部RA状態を伴うため本ローカル媒体検査には含めない。

## 6. Phase 5残件

媒体交換とrollbackは
[15_Phase5媒体交換実施記録.md](15_Phase5媒体交換実施記録.md)で実装した。
Phase 5はまだ完了扱いにしない。残件は次のとおりである。

- 起動時offline、識別失敗、session開始失敗を区別するOffline session状態機械。
- Active中切断・再接続と「起動時Offlineから途中復帰しない」試験。
- hash library、game titles、all user progressによるライブラリ同期。
- 媒体競合解決、title更新、`progress`更新、同期時刻のtransaction管理。
- Full Speed、描画skip、実VM複数frameについての計数可能なintegration test。
- ASCENDの実RA Game ID、タイトル、実績一覧取得とrequest packet確認。

Phase 5の次の実装単位は、起動時を含むOffline session状態機械である。
その後にライブラリ同期を行う。

## 7. 後続Phase

- Phase 6: UI、画像cache、全入力、視覚受入を完了する。
- Phase 7: RA専用Softcore state、progress chunk、検証付きloadを実装する。
- Phase 8: Hardcore policyとmacOS完全受入を行う。
- Phase 9以降: Windows、Linux、Android、4 OS最終受入の順序を維持する。

## 8. 次回作業handoff（更新前記録）

以下は媒体交換実装前のhandoffとして保存する。現在の次回作業は
[15_Phase5媒体交換実施記録.md](15_Phase5媒体交換実施記録.md)の
「次回作業handoff」を正とする。

### 8.1 現在地点

- branch: `codex/retroachievements-integration`
- 実装commit: `d5529c0 Add RA Rich Presence menu and frame callback support`
- `origin/codex/retroachievements-integration`へ反映済み。
- ASCENDによる実フレーム評価とRich Presence表示は利用者手動確認済み。
- 次回はPhase 5のmedia change/rollbackから開始する。Library同期やPhase 6のtoast
  queueへ先に進まない。

### 8.2 最初に実装する単位

`rc_client_begin_change_media()`を使う「同一ゲームの別D88への交換」と、その失敗処理を
1単位として実装する。

処理順:

1. 変更先mediaが現在のLibrary `games.id`に所属することをDBで確認する。
2. 保存済みRA識別hashとworking copyを取得し、D88 probeとbank範囲を検証する。
3. 現在のDrive 1 path、bank、RA hashをrollback用に保持する。
4. 新hashで`rc_client_begin_change_media()`を開始する。
5. RA成功時だけVMのDrive 1をworking copyへ交換する。
6. VM交換失敗時は旧hashへ`rc_client_begin_change_media()`してRAをrollbackする。
7. rollback失敗時はRA評価を停止してOffline sessionへ移し、VMは交換前媒体を維持する。

境界条件:

- 同一D88内のbank切替ではmedia change APIを呼ばない。
- Drive 2の変更ではmedia change APIを呼ばない。
- 別ゲーム所属mediaは現在sessionへ挿入せず、ゲーム再起動を要求する。
- RA変更確定前にVMの現在媒体やlaunch profileを変更しない。

### 8.3 主な変更対象

- `Source/RA/ra_service.h/.cpp`
  - media changeのpending/callback/abort、成功・失敗snapshot、rollback入口。
- `Source/UI/app.cpp/.h`
  - `OpenDiskFromUser()`とRA媒体解決を、load gameとmedia changeで分岐する。
  - RA成功後のVM交換、VM失敗時rollback、Offline遷移を調停する。
- `Source/RA/ra_library.h/.cpp`または`ra_media_store.h/.cpp`
  - 対象mediaの同一game所属、hash、working path、bankを一括検証して返す。
- `Tests/ra_service_test.cpp`、`Tests/ra_library_store_test.cpp`
  - 下記の成功・失敗経路をfake client/生成fixtureで固定する。

### 8.4 完了条件

- 同一ゲーム別媒体のRA成功後だけDrive 1が交換される。
- RA change失敗時はVMとlaunch profileが変化しない。
- VM交換失敗・RA rollback成功時は旧媒体で評価を継続する。
- rollback失敗時はOffline sessionとなり、以降`DoFrame()`しない。
- bank切替とDrive 2変更でmedia change API呼出しが0回である。
- 別ゲーム媒体が拒否され、現在sessionとVM媒体が維持される。
- shutdown時にpending media change callbackを残さない。

ASCENDは単一D88なので回帰確認へ使う。媒体交換の自動・手動確認には
`ra_seed_library_fixture`が生成する`RA Test Multi Disk`を使う。

再検証command:

```sh
cmake --build build-ra --target xm8 ra_service_test \
  ra_library_store_test ra_seed_library_fixture_test
ctest --test-dir build-ra \
  -R 'host_frame_callback_test|ra_|d88fixture_test|d88probe_test|clidisk_test' \
  --output-on-failure
cmake --build /tmp/xm8mac-ra-audit-off --target xm8 \
  host_frame_callback_test d88fixture_test d88probe_test clidisk_test
ctest --test-dir /tmp/xm8mac-ra-audit-off \
  -R 'host_frame_callback_test|d88fixture_test|d88probe_test|clidisk_test' \
  --output-on-failure
git diff --check
```

media change/rollback完了後に、Offline session状態機械、Library同期の順で進める。
