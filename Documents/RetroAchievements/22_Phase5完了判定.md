# Phase 5 完了判定

## 1. 判定

macOS RAセッションとフレーム評価について、工程表の実装項目、自動テスト、実RAサービスを
使う手動受入を完了したため、Phase 5を完了とする。

## 2. 完了根拠

- hashによるゲームload、media change、unload、失敗時Offlineを実装済み。
- hash library、game title、all user progressの3段階Library同期を実装済み。
- 実VMの完了frameごとに`rc_client_do_frame()`を1回呼ぶことをintegration testで固定済み。
- 描画skip、Full Speed相当、1 sound生成内の複数frameでも計数一致を確認済み。
- 媒体交換のVM失敗時rollbackと、rollback不能時Offlineを確認済み。
- 媒体競合の起動禁止、明示MERGE／SPLIT、MANUAL拒否を確認済み。
- ASCENDの実RAセッションでGame ID、タイトル、実績系表示が動作することを確認済み。
- 実HTTPS Library同期後のtitle、badge、progress表示を確認済み。
- macOSのネットワーク切断・復旧がresetなしで表示へ反映されることを確認済み。
- RA request生成境界にはD88本体やローカルpathを渡さず、bank hashだけを渡す。

テスト媒体:
`/Volumes/CrucialX6/roms/PC88/TEST/ascend.d88`

- size: 348848 bytes
- banks: 1
- RA hash: `5def00835e061a54fe6d1fa2d5a8d2b0`

## 3. 自動回帰

最終実装時点で次を確認した。

- RA ON build: 成功。
- RA ON test: 19件中19件成功。
- RA OFF build: 成功。
- RA OFF test: 9件中9件成功。
- 接続状態機械のAddressSanitizer／UndefinedBehaviorSanitizer test: 成功。
- `git diff --check`: 成功。

## 4. Phase 6 handoff

次はmacOSオーバーレイと画像の仕上げへ進む。

1. toast queueの表示競合、優先順位、寿命は
   [23_Phase6通知Queue実施記録.md](23_Phase6通知Queue実施記録.md)で実装した。
2. badge画像cacheの永続化、容量上限、破損・失敗時placeholderを完成させる。
3. keyboard、mouse、controller、touchの全導線を確認する。
4. Library、Game Detail、Achievements、Leaderboardsの視覚受入を行う。
