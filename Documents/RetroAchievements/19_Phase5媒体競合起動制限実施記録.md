# Phase 5 媒体競合起動制限実施記録

## 1. 今回の範囲

Library同期で`games.identification_state=4`となった媒体競合ゲームを画面へ表示し、
競合解消前にはRAゲームとして起動できない安全境界を実装した。

統合・分離transactionは次の作業単位とする。安全境界を先に固定することで、解消UIの
途中実装やUI以外の呼出しから競合ゲームが起動されることを防ぐ。

## 2. Library表示

- Library v1は識別済みゲームに加えて媒体競合ゲームを一覧へ出す。
- 競合行はタイトルの前に`[!]`を表示する。
- Game Detailは`RA ID conflict`と理由を表示する。
- STARTボタンは`CONFLICT`へ置き換え、keyboard、mouse、touchから起動actionを生成しない。
- 未照合、RA未登録、照合エラーは従来どおりLibrary v1へ表示しない。

## 3. 起動制限

起動可否はUIだけに依存しない。

- `RaLibrary::LoadGameIdentification()`でGame IDと状態を一括取得する。
- `App::LaunchRaLibraryGame()`は媒体操作前に識別済み状態を検証する。
- `RaMediaStore::ResolveLaunchProfile()`も同じ検証を行い、直接呼出しを拒否する。
- 競合時の共通エラーは`media conflict must be resolved before launch`とする。
- 未照合、RA未登録、照合エラーには
  `game is not identified for RetroAchievements`を返す。

これにより、競合ゲームではworking copyの解決、Drive交換、VM reset、RA session開始、
最終起動日時更新のいずれも実行されない。

## 4. 自動test

- 競合ゲームがLibrary一覧から消えず、状態4として返ること。
- 競合ゲームの`ra_game_id`が未確定であること。
- media storeのlaunch profile解決が明示理由で失敗すること。
- Game DetailのEnterが起動actionを返さないこと。
- 識別済みゲームの既存START操作は維持されること。

2026-07-18の検証結果:

- RA ON app build: 成功。
- RA ON test: 19件中19件成功。
- RA OFF app build: 成功。
- RA OFF test: 9件中9件成功。
- `git diff --check`: 成功。
- ASCEND: 348848 bytes、1 bank、RA hash
  `5def00835e061a54fe6d1fa2d5a8d2b0`で不変。

## 5. 次回作業

次は競合を次の2種類へ分類し、Game Detailから明示操作できるようにする。

1. 同じRA Game IDへ解決された複数ローカルゲーム: 古いゲームへ`MERGE`する。
2. 1ローカルゲーム内の媒体が異なるRA Game IDへ解決: 媒体単位で`SPLIT`する。

いずれも単一transactionで媒体ordinal、launch profile、旧ゲーム削除、新ゲーム作成を
完結させる。1つのマルチbank D88自体が複数RA Game IDを含む場合は媒体単位で分割できないため、
自動SPLIT対象にせず手動媒体構成が必要と表示する。
