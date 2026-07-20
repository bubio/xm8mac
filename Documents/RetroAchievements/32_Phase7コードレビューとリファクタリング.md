# Phase 7コードレビューとリファクタリング

## 1. 結論

2026-07-20、Hardcore実装へ進む前にPhase 7のRA Casual／Offline state差分をレビューした。
Hardcore着手を止める未解決事項はない。レビューで検出した安全性上の問題2件を修正し、state
formatとI/Oの異常系試験を補強した。

## 2. 検出・修正事項

### 2.1 RAセッション維持経路の公開範囲

VM再構築時にRAセッションを維持する引数が公開`App::ChangeSystem()`に置かれていた。このままでは
将来の呼出し追加時に、通常のsystem変更やHardcore制約から意図せずRA lifecycle処理を迂回できる。

公開APIは従来の`ChangeSystem(bool load = false)`へ戻し、RA state bodyの復元だけがprivate
`ChangeSystemInternal(..., preserve_ra_session)`を呼べる構造に変更した。セッション維持は検証済み
RA state load内部に閉じている。

### 2.2 state書込み失敗の検出

従来の`FILEIO`はopen成否しか保持せず、途中のshort writeやclose失敗を呼出し側が判定できなかった。
容量不足などで不完全なstate bodyが生成されても、RA containerへ確定する可能性があった。

`FILEIO::HasError()`を追加し、primitive read/write、bulk read/write、seek、tell、closeの失敗を保持する
ようにした。Normal stateは失敗を成功として返さず、RA stateはbodyが完全に書けた場合だけCRC付き
containerを組み立ててatomic renameする。RA load前のrollback用state作成にも同じ判定を適用し、
rollback自体が失敗した場合はRA評価を停止してOffline sessionへ移る。

## 3. リファクタリングと防御強化

- 既存Normal stateの直列化処理を`SaveStateBody()`／`LoadStateBody()`へ集約し、NormalとRAで順序を共有。
- RA progress最大値をstate storeと`RaService`で同じ定数へ統一。
- state全体を256MiB、progressを16MiB、slotを0〜9へ制限。
- state formatの固定offsetを名前付き定数へ置換し、未知modeと予約byteをCRC検証後にも明示拒否。
- directory作成時の`EEXIST`が通常fileだった場合を拒否。
- build/read/serialize失敗時に出力bufferへ古い値を残さない契約へ統一。
- `FILEIO::HasError()`をconst化。

## 4. 試験

追加・強化した試験:

- `fileio_error_test`: 正常read/write、EOFによるshort read、異常状態の保持、再open時clear。
- `ra_state_store_test`: round trip、CRC・size・version・mode・予約byte破損、Game ID・媒体・
  rcheevos version不一致、slot範囲、atomic replacement、失敗時output clear。
- `ra_service_test`: 未load時のserialize拒否とoutput clear、実ゲームprogressのserialize、破損拒否、復元。

最終結果:

```text
RA ON Debug build + ctest:  22/22 passed
RA OFF Debug build + ctest: 10/10 passed
ASan/UBSan RA ON全試験:     22/22 passed
git diff --check:            passed
```

既存core由来のcompiler warningは残るが、本レビュー差分に起因するbuild error、test failure、
AddressSanitizer／UndefinedBehaviorSanitizer検出はない。

## 5. 手動確認

利用者はレビュー前のPhase 7実装について、ASCENDでRA Casual Save／Loadと、RA mode有効のまま
終了・再起動してもNormal slot 0を自動loadしないことを確認済みである。

今回の修正は失敗経路とAPI境界が中心で、正常系の画面仕様は変更していない。Hardcore着手前の任意の
回帰確認として、RA CasualのSave／Loadを1回、NormalのSave／Loadを1回確認すれば十分である。
