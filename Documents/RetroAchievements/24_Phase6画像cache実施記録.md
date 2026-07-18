# Phase 6 画像cache実施記録

## 1. 実装範囲

Libraryの既存`image_cache` tableと`images`ディレクトリを使い、
RAゲームと実績のbadge画像を永続化した。

- 描画要求時はメモリ、永続cache、HTTPSの順で参照する。
- ファイル名はDB採番IDだけから`images/<id>.png|jpg`を生成する。
- URL内のpath要素はローカルファイル名に使用しない。
- `image/png`と`image/jpeg`だけを許可し、parameterと大文字小文字を正規化する。
- 保存前と読込時の両方でstb_imageによる実デコードを行う。
- 符号化済み画像は1MiB、寸法は2048x2048、ピクセル数は
  4,194,304を上限とする。
- ファイル消失、size不一致、破損画像、不正なrelative pathは
  cache missとし、対応するDB行とファイルを削除する。
- 設定済み64/128/256MiBを上限に使い、`last_used_at`が古い順に削除する。
- 直近1秒以内に描画した画像は容量削除の対象外とする。
- 未取得、HTTP失敗、破損時はGame、Badge、Lockedの種類別
  placeholderを表示する。

## 2. 自動テスト

`ra_library_store_test`で次を検証する。

- cache missと保存後のhit。
- Library DBを閉じて再オープンした後のhit。
- Content-Type正規化と画像bytesの保持。
- hit時の`last_used_at`更新とLRU eviction。
- 現在使用中URLを保護したeviction。
- 破損ファイルのmiss化とDB行の自動削除。
- 不正Content-Typeと1MiB超過の拒否。

RA有効構成19テストとRA無効構成9テストはすべて成功した。
`ra_library_store_test`はAddressSanitizerとUndefinedBehaviorSanitizerを有効にした
ビルドでも成功した。

## 3. 手動受入

2026-07-18に利用者が実際のbadgeで次を確認した。

- 取得したbadgeが永続化される。
- 永続化後はbadgeが待ち時間なく即時表示される。

これにより、実HTTPS取得から永続cache hit、App描画までの受入を
完了とする。

## 4. 次回作業handoff

入力導線の監査と補完は
[25_Phase6入力導線実施記録.md](25_Phase6入力導線実施記録.md)で実施した。
入力導線の補完後は、Phase 6の視覚受入と通知UI残件へ進む。
