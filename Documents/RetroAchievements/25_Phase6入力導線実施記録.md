# Phase 6 入力導線実施記録

## 1. 監査結果

Library、Game Detail、Achievements、Achievement Detail、Leaderboards、Loginの
入力経路を画面と入力方式ごとに監査した。

| 操作 | Keyboard | Mouse | Controller | Touch |
|---|---|---|---|---|
| 一覧移動 | 矢印、Tab、PageUp/Down | hover、wheel | D-pad/左stick、LB/RB | tap、drag scroll |
| 決定 | Enter | 左click | A | tap |
| Back | Esc | 右click、X1 | B、Back | OS Back |
| Login focus | Tab、矢印、PageUp/Down | click | D-pad、LB/RB | tap |
| Login文字 | SDL text input | OS text input | 未実装 | OS text input |

## 2. 補完した経路

- KeyboardのPageUp/PageDownで一覧と実績詳細を1page移動する。
- ControllerのLB/RBをPageUp/PageDownへ接続する。
- Library、Achievements、Leaderboardsはpointer downとupが同じ行の場合だけ
  決定する。
- 詳細画面のSTART/MERGE/SPLITもpointer down/upの一致を必須とする。
- touch scroll後の指離しは決定として扱わない。
- 既存メニュと同じく、keyboardのBackはEscのみとする。Backspaceと詳細titleは
  Back操作に割り当てない。

## 3. 検証

`ra_overlay_test`に次を追加した。

- PageUp/PageDownによる一覧選択移動。
- 実績詳細のpage scroll。
- 一覧のBackspaceで画面をcloseしないこと。
- Login focusのPageUp/PageDown移動。
- 表示行座標から対応するlist targetを得られること。

RA有効構成19テストとRA無効構成9テストはすべて成功した。
`ra_overlay_test`はAddressSanitizerとUndefinedBehaviorSanitizerを有効にした
ビルドでも成功した。

## 4. Login文字入力の扱い

RAで文字入力が必要なのはLoginのUsernameとPasswordだけである。PC環境は
物理キーボードを前提とする。AndroidはまずOS software keyboardを使用し、表示、
フォーカス、入力、確定、Back、画面の可視性に支障がないことを実機で確認する。
支障がある場合はRA独自キー配列ではなく、Android APIによるLogin専用native画面を
実装する。Android実機検証まで追加実装は行わない。

## 5. 次回handoff

次はPhase 6の視覚受入と通知UI残件へ進む。640x400とmacOS各倍率で、
toast、badge、長文、scroll、placeholderの重なりとclipを確認する。
