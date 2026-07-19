# Phase 6 動的ステータス行表示実施記録

## 1. 実施日と対象

- 実施日: 2026-07-19
- 対象: macOS基準実装
- 計画: [26_Phase6動的ステータス行表示計画.md](26_Phase6動的ステータス行表示計画.md)
- テスト媒体: `/Volumes/CrucialX6/roms/PC88/TEST/ascend.d88`

## 2. 実装結果

### 2.1 通知と状態page

- 通知の同時表示を1件に変更した。優先度、FIFO、中断・残り時間からの再開、64件上限、
  3/5/8秒の設定寿命は維持している。
- Challenge、Progress、Leaderboard trackerを独立した状態pageとして保持する。
- page順をChallengeのSHOW順、単一Progress、trackerのSHOW順に固定した。
- SHOWは対象pageを即時選択し、UPDATEは内容だけを変更し、HIDEは即時削除する。
- 複数pageは3秒ごとに切り替える。通知またはフルオーバーレイの表示中はpageの選択と
  残り切替時間を保持する。
- Progressの寿命はXM8側で作らず、`rc_client`のSHOW／UPDATE／HIDEをそのまま反映する。
  通知やフルオーバーレイに隠れている間のHIDEも即時削除する。
- 実績解除通知にはbadge URLとポイントを含める。Rich Presenceは従来どおりメニューの
  `Now:`専用で、通知または状態pageにはしない。
- 現在のRAサービスはPhase 8までSoftcore固定のため、解除通知へmode labelは表示しない。
  Hardcore有効化後に実際のセッションmodeに基づく表示を追加する。

### 2.2 ステータス行

- RA表示中は640x16の全幅を専有し、ディスク、fps、NOWAIT、mode/clockを一時的に隠す。
- RA表示終了時は通常ステータス全体を強制再描画する。
- `Status area`有効時は画面外の専用行、無効時はゲーム画面下端16pxへ既存設定のalphaで
  重畳する。
- 一時通知を状態pageより優先し、画面へ表示するRA行は常に1本だけにした。
- ChallengeとProgressはbadge URLが空、未取得、破損のいずれでも16x16領域を確保し、
  placeholder枠で文字開始位置を固定する。
- 表示文はShift-JISへ変換し、行幅を超える部分は描画矩形でclipする。横scrollは行わない。

### 2.3 入力

- `Status area`有効時だけ専用行をmouse左clickまたはtouch tapすると、次のpageへ切り替える。
- down/upが同じ行内で、dragがなく、通知なし、pageが2件以上の場合だけ切替を実行する。
- 通知または1pageの行は入力を消費するが、dismissや別動作は行わない。
- `Status area`無効時はRA行をhit test対象にせず、mouse／touchをVMへ渡す。
- keyboardとcontrollerへ新しい切替操作は追加していない。

## 3. 自動検証

2026-07-19の最終差分で次を実施した。

```text
cmake --build build-ra -j2
ctest --test-dir build-ra --output-on-failure
  -> 19/19 passed

cmake --build build -j2
ctest --test-dir build --output-on-failure
  -> 9/9 passed

clang++ -std=c++17 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I Source/RA Tests/ra_overlay_test.cpp Source/RA/ra_overlay.cpp \
  -o /tmp/xm8-ra-overlay-sanitize
/tmp/xm8-ra-overlay-sanitize
  -> passed（ASan／UBSan指摘なし）

git diff --check
  -> passed
```

`ra_overlay_test`では次を追加確認した。

- 通知が常に1件であること、優先度中断、pause、tick wrap、queue上限
- Challenge／Progress／trackerの順序、SHOW即時選択、UPDATE、HIDE
- 3秒境界、手動next、pause/resume、32bit tick wrap
- UPDATEが3秒timerを再開しないこと
- pause中のProgress HIDEが復帰後に再表示されないこと
- session stop相当の全状態clear

`ra_service_test`ではProgress UPDATEとLeaderboard tracker SHOWのevent内容がdeep copyされ、
UI層へ正しく渡ることを確認した。RA無効buildもcompile/linkと全Normal testに成功している。

## 4. 手動確認の状態

自動起動によるASCEND確認を試みたが、開発buildが保存済みRA認証情報へアクセスする際の
macOSキーチェーン許可画面で停止した。認証情報を扱う画面のため、自動でパスワード入力や
許可回避は行っていない。

ユーザー環境でRA buildを起動し、キーチェーンアクセスを許可したうえで、次を確認した。

1. ASCENDで通知が従来のゲーム画面上部ではなく、ステータス行全幅へ1件だけ表示される。
2. Challenge、Progress、Leaderboard trackerが重なった場合、3秒ごとに1件ずつ切り替わる。
3. `Status area`有効時のclick／tapで即時切替でき、無効時は下端操作がVMへ届く。
4. 実績解除通知の終了後に有効pageへ戻り、全page消滅後は通常ステータスが欠けずに戻る。

2026-07-19、上記の動作に問題がないことをユーザーが確認した。

詳細な倍率、Progressの約2秒HIDE、複数tracker等の受入項目は
[26_Phase6動的ステータス行表示計画.md](26_Phase6動的ステータス行表示計画.md)の7章を使用する。

## 5. 判定

- 実装: 完了
- 自動検証: 完了
- ASCEND実画面受入: 完了
- Phase 6動的ステータス行表示の完了判定: 完了

## 6. 手動受入後の修正

2026-07-19、ASCENDで実績解除通知自体は表示されたが、16x16 badgeが単色の四角に見えることを
確認した。原因は、badge画像の描画後に`Font::DrawRect()`を呼び、枠の内部色で画像全体を
上書きしていたことだった。

placeholder枠を先に描き、その後にbadge画像を重ねる順序へ修正した。画像取得待ちまたは取得
失敗時はplaceholder枠を維持し、取得完了時は同じ16x16領域へ画像を表示する。文字開始位置は
どちらの状態でも変化しない。

修正後、実績解除通知に縮小されたbadge画像が表示されることをユーザーが再確認し、問題なしと
判定した。

## 7. 単純な状態通知の削減

2026-07-19、ゲーム中の操作を妨げる単純な状態通知を削減した。

- 削除: `identifying`、`loading game`、ログイン開始、Library同期中／成功、ゲーム起動成功、
  RA mode切替成功
- 維持: ログイン結果、ゲーム識別完了、各種エラー、切断・復旧、実績解除・完了、
  Leaderboard結果
- 状態pageとして維持: Challenge、Progress、Leaderboard tracker

識別成功通知は従来の`RA: <title>`から`RA: identified <title>`へ変更し、何が完了したのかを
明示した。媒体rollback、競合解消、logoutなど、明示操作の結果を伝える通知は維持している。
