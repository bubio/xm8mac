# macOS総合回帰修正記録

## Cursor Key to Num Padの起動時適用

2026-07-21、`Cursor Key to Num Pad`を保存済みONにしていても、RAモードでアプリを
再起動すると設定表示だけがONで、実際のキーマップが有効にならない問題を確認した。
Input SettingsでいったんOFF、再度ONにすると有効になっていた。

原因は、保存済みのkeyboard remapが`Input::Init()`では適用されず、通常モードの起動時
auto-state loadに含まれる`LoadStateBody()`からだけ再適用されていたことである。RAモードは
Normal stateとの混在を防ぐためauto-state loadを意図的に行わず、この副作用も失っていた。

修正ではRA起動経路に例外処理を追加せず、`Input::Init()`で次の保存済み設定を常に適用する。

- `Cursor Key to Num Pad`
- `Num Key to Num Pad`

これによりNormal、RA OFF、RA Softcore、RA Hardcore、起動stateの有無に依存せず、Input初期化
直後から同じ設定になる。既存のstate load後の再適用は、state内設定を反映するため維持する。
メニュー表記の`Coursor`も`Cursor`へ修正した。

## 自動検証

- RA ON Debug build／全試験: 23/23 passed
- RA OFF Debug build／全試験: 10/10 passed
- ASan／UBSan RA ON build／全試験: 23/23 passed
- `git diff --check`: passed

## 手動受入

次の確認が完了するまで本項目は手動受入待ちとする。

1. RAモードをONにし、`Cursor Key to Num Pad`をONにする。
2. アプリを終了して再起動する。
3. Input Settingsを開き直す操作を行う前に、カーソルキーがNum Padとして動作することを確認する。
4. Input Settingsでは設定がONのままであることを確認する。
5. 可能なら`Num Key to Num Pad`でも同じ再起動確認を行う。
