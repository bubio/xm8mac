# Hardcore初期値とメインメニューRA表示

## 1. 決定

2026-07-20、RetroAchievementsの推奨に合わせ、新規環境で最初に選択されるプレイモードを
SoftcoreからHardcoreへ変更した。RA機能全体の初期値はOFFのままとする。

- 新規環境: `RA mode = OFF`、選択済みプレイモードは`Hardcore`
- 既存環境: `library.sqlite3`に保存済みのRA有効状態とプレイモードを維持
- 既存schemaで設定行だけが欠けている場合: 新しい設定行をHardcoreで生成

## 2. メインメニュー表示

メインメニューのタイトルを次の形式へ変更した。

```text
<< XM8 Ver 1.79 :  [RA STATUS] >>
```

`RA STATUS`には設定状態に応じて次のいずれかを表示する。

- RA無効: `RA OFF`
- RA有効かつSoftcore選択: `RA SOFT`
- RA有効かつHardcore選択: `RA HARD`

RA非対応ビルドでは常に`RA OFF`と表示する。通信切断やゲーム識別中などの一時的な
セッション状態はこのタイトルへ混在させず、従来どおり通知およびステータス行で扱う。

## 3. 実装と検証

- `RaSettings`、SQLite schema、設定行の補完処理、AppのfallbackをHardcore初期値へ統一
- 設定保存は`INSERT OR IGNORE`を維持し、既存行を上書きしない
- 新規DBのRA無効／Hardcore初期値を自動試験で確認
- 保存済みモードがDB再open後も維持される既存試験を回帰確認
- RA ON Debug build／全試験: 23/23 passed
- RA OFF Debug build／全試験: 10/10 passed
- ASan／UBSan RA ON全試験: 23/23 passed
- `git diff --check`: passed

手動確認では、メインメニューを開き、RA OFF、RA SOFT、RA HARDの各選択に対応して
タイトル末尾の表示が切り替わることを確認する。
