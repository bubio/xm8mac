# Phase 6 通知Queue実施記録

## 1. 実装範囲

最後の1件で上書きしていたRA通知を、優先度付きtoast queueへ置き換えた。

- 優先度はCritical、Important、Normal、Lowの4段階。
- 実績解除、Game completed、Subset completedをCriticalとする。
- Leaderboard結果、切断・復旧、server errorをImportantとする。
- 同時表示は最大3件、残りはqueueで待機する。
- 同優先度は到着順を維持する。
- 高優先度が到着した場合、低優先度を残り時間付きで待機へ戻す。
- 高優先度終了後、退避した通知を残り時間から再開する。
- フルオーバーレイ中はtoastを隠し、表示時間を消費しない。
- 保存済み設定のNotification duration 3/5/8秒を使用する。
- 内部queueは64件を上限とし、超過時は低優先度の新しい通知から破棄する。
- ゲームセッション停止時は、表示中と待機中の通知をすべて破棄する。

toastは画面右下から上方向へ最大3件を配置し、既存ステータス行の上に収める。

## 2. テスト

`ra_overlay_test`で次を検証する。

- 空通知とduration 0の拒否。
- 同時表示3件と4件目の待機。
- 優先度順と同優先度FIFO。
- CriticalによるLowの一時退避と再昇格。
- フルオーバーレイ相当のpause中に非表示・時間停止すること。
- pause中に追加した通知が復帰後に表示されること。
- SDL tickの32bit wrap境界でも期限判定できること。
- queueが64件を超えないこと。
- 通知だけを破棄してもオーバーレイのsnapshotが保持されること。
- `Clear()`が通知と画面状態を消去すること。

RA有効構成の19テスト、RA無効構成の9テストはすべて成功した。
`ra_overlay_test`はAddressSanitizerとUndefinedBehaviorSanitizerを有効にした
C++17ビルドでも成功した。

## 3. 通知関連の残件

priority queueと寿命管理は完了したが、通知UI全体には次が残る。

- 実績解除toastのバッジ、ポイント、Hardcore状態表示。
- ゲーム終了後に保持する送信エラー履歴。
- Challenge Indicator、Leaderboard tracker実装後の領域競合制御。
- 640x400とmacOS各倍率での最終screenshot受入。

## 4. 次回作業handoff

badge画像cacheは[24_Phase6画像cache実施記録.md](24_Phase6画像cache実施記録.md)で
永続化した。次は全入力導線の監査と補完へ進む。
