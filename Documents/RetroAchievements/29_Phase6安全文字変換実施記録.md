# Phase 6 安全文字変換実施記録

## 1. 方針

RA画面はPC-88 KANJI ROMの字形を使う既存`Font`で描画するため、RAサーバーのUTF-8文字列を
描画直前だけShift-JISへ変換する。新しいrenderer、OS font、SDL_ttfは追加しない。

`RaTextConverter`は次だけを担当する薄いhelperとして実装した。

- UTF-8の厳密な構文検証
- overlong、surrogate、途中終端、不正continuationの置換
- 4-byte UTF-8、埋め込みNUL、改行等の制御文字を`?`へ置換
- 置換件数の返却（元文字列は診断情報へ含めない）
- Shift-JISの1/2byte境界、文字数、8/16px表示幅、文字indexからbyte位置の計算

字形へのmapping、macOSのNFC正規化、実描画は既存`Converter`と`Font`を使用する。DB、RA event、
Library内の所有文字列はUTF-8のまま保持する。

## 2. 接続範囲

RAのLibrary、Achievements、Achievement Detail、Leaderboards、Rich Presence、通知、Challenge、
Progress等が共通利用する`ToSjisMenuText()`の前段へ接続した。トップ階層RAメニューのゲームtitleと
Rich Presenceも同じ変換を通す。

clip、wrap、選択行の横scrollが使用していたローカルShift-JIS境界処理も同helperへ集約した。

## 3. 自動検証

`ra_text_converter_test`を追加し、次を検証した。

- ASCIIと日本語UTF-8を変更しない
- 絵文字を1つの`?`へ置換
- 埋め込みNULと制御文字の置換
- overlong、surrogate、途中終端を安全に1 replacement sequenceとして処理
- Shift-JISの半角、全角、不完全な末尾lead byteの文字数と表示幅
- 文字indexからbyte offsetを求めても全角文字の途中を指さない

2026-07-19にRA有効構成20/20、RA無効構成9/9の全テストを実施した。
`ra_text_converter_test`はAddressSanitizerとUndefinedBehaviorSanitizerを有効にした単体実行でも
成功した。

## 4. 判定

- 安全なUTF-8前処理: 完了
- Shift-JIS文字境界・幅計算の共通化: 完了
- 既存KANJI ROM描画の維持: 完了
- 新しいfont／renderer依存: 追加なし

次はPhase 6全体の完了監査を行う。
