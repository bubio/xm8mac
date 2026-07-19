# Phase 6 完了監査

## 1. 監査範囲

2026-07-19、[05_実装工程とテスト計画.md](05_実装工程とテスト計画.md)のPhase 6、
[03_オーバーレイUI仕様.md](03_オーバーレイUI仕様.md)、Phase 6の各実施記録を現行実装と
自動テストへ再突合した。古い実施記録の残件一覧は作成時点の履歴として保持し、本書を現在の
完了判定とhandoffの基準にする。

## 2. 突合結果

| 項目 | 判定 | 根拠 |
|---|---|---|
| Login、Library、Game Detail、Achievements、Leaderboards | 完了 | 共通overlay、keyboard／mouse／controller／touch試験、実HTTPS受入 |
| 通知queue | 完了 | 全幅ステータス行で同時1件、優先度、pause、tick wrap、64件上限 |
| badge画像 | 完了 | 永続cache、破損復旧、LRU、実績解除時の実画像を利用者確認済み |
| Challenge／Progress／Leaderboard tracker | 完了 | SHOW／UPDATE／HIDE、3秒切替、click／tap、通知中断復帰を自動・手動確認済み |
| Rich Presence | 完了 | RAメニュー専用行、選択時の下部全文scrollを利用者確認済み |
| RA由来文字列 | 完了 | UTF-8厳密検証、Shift-JIS安全変換、実RA表示を利用者確認済み |
| 実績解除通知 | 完了 | badge、title、pointsを表示。Hardcore labelは実セッションmodeを実装するPhase 8対象 |
| 送信エラー1件保持 | 完了 | `RC_CLIENT_EVENT_SERVER_ERROR`のAPI、関連ID、messageをdeep copyし、次ゲーム開始まで保持 |
| HTTP応答境界 | 完了 | transport byte数を維持しつつ`rcheevos`へ渡すbuffer末尾へNUL sentinelを保証 |
| Android software keyboard | 対象外 | Android実機判定はPhase 11。独自オンスクリーンキーボードは実装しない |

## 3. 送信エラー保持の補完

`rcheevos v12.3.0`の`RC_CLIENT_EVENT_SERVER_ERROR`は、実績解除送信
`award_achievement`またはLeaderboard送信`submit_lboard_entry`が再試行されず失敗した場合に
発生する。最新1件を次のように扱う。

- 一時通知は従来どおり優先度付きqueueへ表示する。
- game stopでは通知と動的pageだけを消し、最新送信エラーはRAメニューに残す。
- RAメニューのstatus行へ送信種別、関連ID、messageを表示する。
- status行選択中はRich Presenceと同じ画面下部領域へ全文を横scroll表示する。
- 次のRAゲーム開始時に古い送信エラーを消す。全RA状態の`Clear()`でも消す。

専用履歴画面や複数件の永続履歴は追加しない。

## 4. HTTP応答境界の修正

RA有効構成全体をAddressSanitizer／UndefinedBehaviorSanitizer付きで新規buildしたところ、
`ra_service_test`のLeaderboard詳細応答でheap buffer over-readを検出した。HTTP層は本文を
`std::vector<uint8_t>`へ正しい長さで保持していたが、`rcheevos`のJSON処理には長さでfieldを
特定した後に、そのfield先頭をlibc文字列関数へ渡す経路がある。このため本文末尾直後にNULが
必要だった。

`RaRcClientHttpBridge`でcallback中だけ有効なNUL終端bufferを作り、`body_length`はtransportから
受信した元のbyte数のまま渡すよう修正した。NULはJSON本文の一部として数えない。
`ra_credentials_http_test`へ、本文内容・長さが不変で`body[body_length]`だけがNULである検証を
追加した。

## 5. 自動検証

2026-07-19に次を実行した。

```text
cmake --build build-ra -j4
ctest --test-dir build-ra --output-on-failure
  -> 20/20 passed

cmake --build build -j4
ctest --test-dir build --output-on-failure
  -> 9/9 passed

RA有効構成全体20件: AddressSanitizer／UndefinedBehaviorSanitizer
  -> 20/20 passed

git diff --check
  -> passed
```

`ra_service_test`へserver errorのAPI、message、related IDのdeep copyを追加した。
`ra_overlay_test`へgame stop相当のclearで最新送信エラーだけを維持し、全clearと次ゲーム開始相当で
消去できることを追加した。

RA有効／無効構成を並行実行した際、従来の`pathresolver_mac_test`が固定一時directoryを共有して
競合することも検出した。各実行でUUID付きdirectoryを使うよう修正し、両構成の並行試験でも
20/20、9/9が同時成功することを確認した。

## 6. 視覚受入

利用者確認済み:

- ASCENDで実績解除通知、badge、動的page切替、通常status復帰
- mouse／touchによるpage切替とStatus area無効時の入力透過
- Rich Presenceの専用行と下部全文scroll
- RA由来文字列の安全表示

最終確認として、macOSの640x400、960x600、1280x800で文字、badge、枠がclipされないことを
確認する。描画自体は640x400論理frameへ行い、各倍率は同じframeをSDLが拡大するため、機能上の
追加分岐はない。

## 7. 倍率受入で検出した1px残留の修正

2026-07-19、利用者が倍率受入中に、RA通知から通常statusへ戻った後、Drive 1とDrive 2の
境界へ通知由来の1pixelが残ることを検出した。通常のdrive panelは区切り線を作るため幅を
`DRIVE_WIDTH - 1`で塗っており、論理x=223とx=447は各widgetの強制再描画だけでは上書き
されないことが原因だった。

RA全幅statusを終了するときは、通常widgetを強制再描画する前にstatus frame全体を現在の
status alphaを使った黒で消去するよう修正した。これにより意図した1pixelの黒い区切りは維持し、
通知の文字・badge画素だけを確実に除去する。RA有効20/20、RA無効9/9、ASan／UBSan付きRA有効
20/20を再実行し、すべて成功した。

2026-07-20、利用者が通知終了後にDrive 1とDrive 2の境界へ画素が残らないことを確認し、
修正済みと判定した。これをmacOS最終倍率受入の完了確認とする。

## 8. 判定と次回handoff

- Phase 6実装: 完了
- Phase 6自動検証: 完了
- macOS最終倍率受入: 完了
- Phase 6完了判定: 完了（2026-07-20）

次はPhase 7のmacOS Softcore stateへ進む。
Phase 7ではNormal stateと別pathを使い、RA runtime progress付きchunk、媒体・Game ID整合性、
破損時のatomic rollbackを実装する。
