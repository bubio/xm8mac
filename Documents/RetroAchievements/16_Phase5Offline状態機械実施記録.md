# Phase 5 Offline状態機械実施記録

## 1. 今回の範囲

RAゲーム実行状態を、独立したboolの組合せではなく次の状態として管理するようにした。

```text
Ready -> Starting -> Active <-> ActiveDisconnected
             \          \
              +----------> Offline

Offline --StopGame--> Ready
```

- 認証、ゲーム識別、session開始の失敗は`Offline`へ遷移する。
- `Offline`では`DoFrame()`と`Idle()`を呼ばない。
- 遅延した成功応答、ログイン成功、再接続イベントでは`Offline`から復帰しない。
- 明示的なReset、RA mode切替、新規Library起動など、ゲームを停止して新しい起動を
  始める操作だけが`Ready`へ戻す。
- 開始済みsessionの一時切断は`ActiveDisconnected`とし、`DoFrame()`を継続する。
- 再接続イベントで`Active`へ戻す。
- logoutで実行中sessionを破棄した場合は、現在のゲームを自動再接続せず`Offline`とする。

## 2. UI

- メニューは`RA: offline for session`または`RA: disconnected`を表示する。
- Offline中に実績・Leaderboard画面を開いた場合は
  `RA offline for this session`を表示する。
- Offline中にログイン操作を行うこと自体は許可するが、成功しても現在のゲームへは
  RA sessionを再接続しない。

Hardcore policy自体はPhase 8で実装予定である。したがって、仕様にあるOffline移行時の
Hardcore表示・制約解除は、Hardcore実装時にこの状態を唯一の判定元として接続し、改めて
受入試験する。

## 3. server eventの扱い

- `DISCONNECTED`: `Active`から`ActiveDisconnected`へ遷移する。
- `RECONNECTED`: `ActiveDisconnected`から`Active`へ遷移する。
- Offline後に到着した両eventは状態も通知も変更しない。
- 個別のAchievement awardやLeaderboard submitに対する`SERVER_ERROR`は、現在の
  RA session全体が無効になったことを意味しないため、自動Offline化しない。
- session継続不能が確定した経路は`SessionInvalidated`でRA gameをunloadし、Offlineへ
  遷移する。現在は媒体rollback不能と実行中logoutがこの経路を使う。

## 4. 実装箇所

- `Source/RA/ra_session_state.h`
  - 状態、signal、遷移、Offline／評価可否判定。
- `Source/UI/app.h/.cpp`
  - 起動処理、frame評価、接続event、menu／overlay表示を共通状態へ接続。
- `Tests/ra_session_state_test.cpp`
  - 開始成功・失敗、遅延成功、切断中評価、再接続、Offline途中復帰禁止。
- `Tests/ra_service_test.cpp`
  - rcheevosの切断・再接続eventが順序どおりApp向けeventへ変換されること。

## 5. 検証結果

2026-07-16にmacOSで確認した。

```sh
cmake --build build-ra --target xm8 ra_session_state_test ra_service_test
ctest --test-dir build-ra --output-on-failure
cmake --build /tmp/xm8mac-ra-audit-off
ctest --test-dir /tmp/xm8mac-ra-audit-off --output-on-failure
git diff --check
```

- RA ON: app build成功、18件中18件成功。
- RA OFF: app build成功、8件中8件成功。
- `git diff --check`: 成功。
- ASCENDのsize、bank数、RA hashは前回から不変。

実ネットワーク確認で、rcheevosイベントだけでは通信要求のない期間の切断を検出できないことが
判明した。macOSの到達性監視を
[21_Phase5接続監視修正実施記録.md](21_Phase5接続監視修正実施記録.md)で追加した。
修正後、ASCENDの実RAセッションでresetなしの切断・復旧表示を確認済みである。
またApp全体をfake HTTPで駆動するtest harnessはまだないため、サービスevent変換と状態遷移を
別々の自動testで固定している。

## 6. 次回作業handoff

Library同期は
[17_Phase5Library同期実施記録.md](17_Phase5Library同期実施記録.md)で実装した。
現在の次回作業は同文書のhandoffを正とする。

Library同期後は、Full Speed／描画skipを含む実VM frame計数test、媒体競合解決の順で進む。
