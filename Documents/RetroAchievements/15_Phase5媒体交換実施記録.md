# Phase 5 媒体交換実施記録

## 1. 今回の範囲

Phase 5残件のうち、RAセッション中のDrive 1媒体交換を実装した。

- 同一Libraryゲームに属する別D88への交換。
- `rc_client_begin_change_media()`成功後だけVM媒体を交換する二段階処理。
- VM交換またはlaunch profile保存失敗時の、VM媒体とRA active mediaのrollback。
- rollback不能時にRA評価を停止するsession限定Offline遷移。
- pending中の二重交換、別ゲーム媒体、shutdown/unload時callbackの防止。

一般的な起動時Offline状態機械、通信切断後の扱い、Library同期は今回の範囲外である。

## 2. 媒体交換の処理順

1. 指定D88をRA作業コピーへ解決し、D88/bankをprobeする。
2. 現在のLibrary `games.id`と対象媒体の所属が一致することを確認する。
3. 現在のDrive 1 path、bank、RA hashをrollback用に保持する。
4. 未確認hashはRA `gameid` APIで解決し、現在ロード中のRA Game IDと一致することを
   確認する。
5. 一致したhashだけを`rc_client_begin_change_media()`へ渡す。
6. RA成功後にDrive 1を対象working copyへ交換し、launch profileを保存する。
7. VM交換またはprofile保存に失敗した場合、旧VM媒体を開き直し、旧hashへRAを
   rollbackする。
8. 旧VM媒体の復元またはRA rollbackに失敗した場合、ゲームをunloadして以降の
   `DoFrame()`を停止し、`RA: offline for session`を表示する。

新hashの事前Game ID照合と`rc_client`内部解決により、初回交換時は同じhashについて
`gameid`要求が2回発生する。これは別ゲーム媒体をVM交換前に確実に拒否するための
現在の安全側の実装である。確認済みhashはsession内でcacheする。

## 3. 境界条件

- Drive 2変更はRA active mediaを変更しない。
- 同一working D88内のbank切替はmedia change APIを呼ばない。
- 現在と同一hashはmedia change APIを呼ばない。
- 別Libraryゲーム所属の媒体は現在のsessionへ挿入せず、再起動を要求する。
- Library所属が一致していても、RAサーバーが別Game IDを返した媒体は拒否する。
- media change pending中はframe評価を一時停止し、VM媒体は変更前のまま保持する。
- `UnloadGame()`と`Shutdown()`はpending callbackをabortする。遅延応答は無視する。

## 4. 実装箇所

- `Source/RA/ra_service.h/.cpp`
  - media change snapshot、Game ID事前照合、callback、abort、hash cache。
- `Source/RA/ra_media_change_policy.h`
  - Drive、pending、Library所属、hashに基づく副作用のない分類。
- `Source/UI/app.h/.cpp`
  - RA成功後のVM commit、VM失敗時rollback、整合不能時Offline遷移。
- `Tests/ra_service_test.cpp`
  - 成功、未知hash失敗、別RA Game ID拒否、既知hash rollback、unload中断。
- `Tests/ra_media_change_policy_test.cpp`
  - 同一ゲーム、pending、別ゲーム、Drive 2、同一hash、bank切替、初回load。

## 5. 自動検証

2026-07-16にmacOSで次を確認した。

```sh
cmake --build build-ra --target xm8 ra_service_test \
  ra_media_change_policy_test
ctest --test-dir build-ra --output-on-failure
cmake --build /tmp/xm8mac-ra-audit-off
ctest --test-dir /tmp/xm8mac-ra-audit-off --output-on-failure
git diff --check
```

- RA ON: app build成功、17件中17件成功。
- RA OFF: build成功、8件中8件成功。
- `git diff --check`: 成功。
- build warningは既存コードのwritable string literal等で、新規エラーはない。

ASCEND回帰:

| 項目 | 結果 |
|---|---|
| path | `/Volumes/CrucialX6/roms/PC88/TEST/ascend.d88` |
| size | 348848 bytes |
| bank数 | 1 |
| bank 0 | `ASCEND` |
| RA hash | `5def00835e061a54fe6d1fa2d5a8d2b0` |

ASCENDは単一媒体なので媒体交換成功経路の手動試験には使えない。実RAサービスを使う
複数ディスクゲームでの交換、VM交換失敗の強制、rollback失敗の強制は未実施である。

## 6. 次回作業handoff

### 6.1 最初に行うこと

この項目は
[16_Phase5Offline状態機械実施記録.md](16_Phase5Offline状態機械実施記録.md)で実装した。
現在の次回作業は同文書のLibrary同期handoffを正とする。

### 6.2 その次の順序

1. hash library、game titles、all user progressによるLibrary同期。
2. 媒体競合解決、title/progress更新、同期時刻transaction。
3. Full Speedと描画skipを含む実VM frame計数integration test。
4. 実RA複数ディスクゲームでmedia change/rollbackを手動確認。
5. Phase 6のtoast queue、画像cache、全入力、視覚受入。

### 6.3 次回の開始時確認

```sh
git status --short
cmake --build build-ra --target xm8 ra_service_test \
  ra_media_change_policy_test ra_session_state_test
ctest --test-dir build-ra \
  -R 'ra_service_test|ra_media_change_policy_test|ra_session_state_test' \
  --output-on-failure
```

媒体交換コードを変更する場合は、RA ON全18件、RA OFF全8件、ASCEND hash不変を
再確認すること。
