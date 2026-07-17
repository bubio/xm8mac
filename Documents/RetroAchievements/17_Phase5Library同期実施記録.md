# Phase 5 Library同期実施記録

## 1. 実装範囲

ログイン後かつゲーム未実行時に、固定版rcheevosの次のAPIを順番に実行するLibrary同期を
実装した。

1. `rc_client_begin_fetch_hash_library(RC_CONSOLE_PC8800)`
2. ローカルhashに一致したGame IDを最大100件ずつ
   `rc_client_begin_fetch_game_titles()`
3. `rc_client_begin_fetch_all_user_progress(RC_CONSOLE_PC8800)`

3応答はメモリ上のsnapshotへコピーし、すべて成功してからSQLiteへ単一transactionで
反映する。途中失敗、応答欠落、重複、不正値、DB制約違反では前回成功値と同期完了時刻を
維持する。

## 2. schema v2

マルチイメージD88をbank単位で照合するため、`media_banks`へ次を追加した。

- `ra_hash`: bankのD88イメージ範囲から計算したRA識別hash。
- `ra_game_id`: hash libraryで得たGame ID。
- `identification_state`: 未照合、対応、未登録、エラー、競合。

schema versionを2へ更新した。v1からの移行では単一bank媒体を`media.md5`でbackfillする。
既存マルチbank媒体は次回のimport／probe時にbank別hashを補完する。未知の新しいschemaを
拒否する既存方針は維持する。

## 3. transaction規則

- hash libraryにない照合済みhashはRA未登録とする。
- hash未補完の旧マルチbank行は未照合のまま維持し、未登録と誤判定しない。
- 同一ローカルゲーム内で複数RA Game IDが見つかった場合は競合としてGame IDを確定しない。
- 複数ローカルゲームが同じRA Game IDへ解決された場合も自動統合せず競合とする。
- anchor bankのGame IDだけを`games.ra_game_id`へ採用する。
- ユーザー編集titleは維持し、それ以外はRA title、sort title、badge URLへ更新する。
- all progressにないpoints値はNULLを維持する。
- progressは`(username, ra_game_id)`単位で置換し、別ユーザー行を削除しない。
- `sync_state`の`library:<username>:pc8800`はtransaction commit時だけ更新する。
- Library画面は現在ログイン中のusernameだけでprogressを絞り込む。ログアウト中は別ユーザーの
  progressを混在表示しない。

## 4. 実行制御

- 自動同期はログイン済み、RA sessionが`Ready`、ゲーム未ロードの場合だけ1回開始する。
- ゲーム開始時はpending同期をabortし、遅延応答を無視する。
- title応答失敗時はprogress APIへ進まない。
- 同期完了時は`RA: library synchronized`、失敗時は
  `RA: library sync failed`を通知する。
- Library画面を開いている場合はcommit後に一覧を更新する。

## 5. 自動test

2026-07-17にmacOSで確認した。

- fake HTTPでhash library、game titles、all progressの順序とrequest種別を確認。
- server hashをローカルhashへ絞り込むことを確認。
- title段階の部分失敗でprogress要求を開始しないことを確認。
- game開始によるabortと遅延hash応答の無視を確認。
- 単一／マルチbank hashのDB登録を確認。
- 全同期成功時のtitle、badge、progress、同期完了行を確認。
- title欠落をDB変更前に拒否することを確認。
- SQLite triggerによる途中失敗でtitleと同期完了行がrollbackされることを確認。
- 2ユーザーのprogress分離と表示時username絞り込みを確認。
- schema v1 fixtureがv2へ移行することを確認。

結果:

- RA ON app build: 成功。
- RA ON test: 18件中18件成功。
- RA OFF app build: 成功。
- RA OFF test: 8件中8件成功。
- `git diff --check`: 成功。
- ASCEND: 348848 bytes、1 bank、RA hash
  `5def00835e061a54fe6d1fa2d5a8d2b0`で不変。

実RetroAchievements HTTPSでの3 API連続同期と画面反映は未確認である。

## 6. 手動確認手順

1. Drive 1を空にした状態でRA modeを有効にする。
2. RAへログインする。
3. `RA: synchronizing library`の後に`RA: library synchronized`が表示されることを確認する。
4. RetroAchievementsメニューからLibraryを開く。
5. 識別済みゲームのRA title、badge、解除数が現在のユーザーの値になっていることを確認する。
6. 同期失敗時は既存Library内容が消えず、再ログインまで自動retryしないことを確認する。

ASCENDを登録済みなら同期後のtitle/progress確認に使用できる。D88本体やローカルpathが
通信requestへ含まれないことも通信診断時に確認する。

## 7. 次回作業handoff

Full Speed、描画skip、1回のsound生成で複数VM frameが進む場合のintegration testは
[18_Phase5実VMフレーム計数実施記録.md](18_Phase5実VMフレーム計数実施記録.md)で完了した。
実PC-8801MA VMの`EVENT::drive()`を使い、完了frame数とcallback回数の一致をRA ON/OFFの
双方で固定している。

現在の順序:

1. 媒体競合を解消する統合・分離UIと競合媒体の起動制限。
2. ASCENDで実HTTPS Library同期を手動確認。
3. 実ネットワーク切断・再接続を手動確認。
4. Phase 5完了判定。
5. Phase 6のtoast queue、画像cache、全入力、視覚受入。

開始時command:

```sh
git status --short
rg -n "identification_state|Conflict|ra_game_id" Source/RA Source/UI Tests
cmake --build build-ra --target xm8 ra_library_store_test ra_service_test
ctest --test-dir build-ra --output-on-failure
```
