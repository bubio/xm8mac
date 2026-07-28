# Phase 5 媒体競合解消実施記録

## 1. 実装範囲

Library同期で検出した媒体競合を安全性に応じて分類し、Game Detailから利用者が
明示実行できる`MERGE`／`SPLIT`を実装した。同期処理自体は所属を変更しない。

| 分類 | 条件 | 操作 |
| --- | --- | --- |
| MERGE | 複数ローカルゲームの全bankが同じRA Game IDへ対応 | 最古のゲームへ統合 |
| SPLIT | 1ゲーム内の各物理D88がそれぞれ単一の別RA Game IDへ対応 | RA Game ID単位へ分離 |
| MANUAL | 1物理D88のbankが複数IDへ対応、または他ゲームを含む複合競合 | 自動解消を拒否 |

## 2. UI

- 競合行を選択してGame Detailを開くと`MERGE`、`SPLIT`、`MANUAL`のいずれかを表示する。
- `MERGE`／`SPLIT`はEnter、クリック、タッチで実行する。
- `MANUAL`は無効表示とし、手動媒体構成が必要であることを本文へ表示する。
- Libraryゲーム実行中は媒体所属を変更せず、停止後の実行を促す。
- 成功時はLibraryをDBから再読込し、`RA: games merged`または`RA: media split`を通知する。

## 3. transaction

`RaLibrary::InspectGameConflict()`はDBのbank別識別結果から毎回分類する。
`RaLibrary::ResolveGameConflict()`は`BEGIN IMMEDIATE`から`COMMIT`までを単一transactionとし、
途中のSQLが1件でも失敗した場合はrollbackする。

MERGEでは最古の`games.id`、その起動構成、RA anchorを維持する。移動媒体のordinalを
末尾へ連番で付け替え、移動元の起動構成とゲームを削除する。

SPLITでは元RA anchorを含む物理媒体を元ゲームへ残す。ほかのRA Game IDごとにゲームを
作成して媒体を移動し、各ゲームのordinalを0から振り直し、Drive 1へRA anchorを1つ作る。
working D88のファイル内容は変更しない。

## 4. 自動テスト

`ra_library_store_test`で次を検証する。

- MERGE／SPLIT／MANUALの分類。
- MERGE後の最古ゲーム・anchor保持、媒体移動、旧ゲーム削除。
- SPLIT後のゲーム数、媒体所属、各ゲームの起動構成とanchor。
- SQL triggerで処理途中を強制失敗させたときの完全rollback。
- 成功後とrollback後の`foreign_key_check`、媒体ordinal、anchor数。
- 成功、失敗、拒否の全経路でworking D88のbyte列が不変であること。

`ra_overlay_test`では識別済みゲームのSTART、解消可能競合の解消action、MANUAL競合の
操作拒否を検証する。

## 5. 後続確認（完了）

ローカルで完結する媒体競合解消に続き、実アカウントと実ネットワークが必要な
次の受入確認も完了した。

1. ASCENDを起動して実RA Game ID、タイトル、実績系表示を確認済み。
2. 実HTTPSのLibrary同期後に、Libraryのtitle、badge、progress反映を確認済み。
3. RAセッション中のネットワーク切断・復旧は、実機確認で見つかった検出不足を
   [21_Phase5接続監視修正実施記録.md](21_Phase5接続監視修正実施記録.md)で修正し、再確認も完了した。

テスト媒体は`/Volumes/CrucialX6/roms/PC88/TEST/ascend.d88`を使用する。外部サービス状態と
利用者資格情報を伴うため、これらはローカル自動テストの合格条件には含めない。
