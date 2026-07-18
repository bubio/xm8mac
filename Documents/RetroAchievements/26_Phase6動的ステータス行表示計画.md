# Phase 6 動的ステータス行表示計画

## 1. 目的

RAの一時通知、Challenge Indicator、Progress Indicator、Leaderboard trackerがゲーム画面を覆い、操作や視認を
妨げることを避ける。従来の右下最大3件toastとゲーム画面内の独立indicator配置を廃止し、
既存の16pxステータス行をRA表示中だけ全面的に使用する。

本計画は[03_オーバーレイUI仕様.md](03_オーバーレイUI仕様.md)の規範仕様を実装するための
作業順、責務分離、テスト、手動受入条件を定義する。

## 2. 確定仕様

### 2.1 表示領域

- RA表示中はステータス行の640x16全体を専有する。
- ディスク、fps、NOWAIT、mode/clock用の領域を予約しない。
- RA表示がなくなると、通常のXM8ステータス全体を直ちに復元する。
- `Status area`有効時はゲーム画面外の専用行へ描画する。
- `Status area`無効時は、RA表示が存在する間だけゲーム画面下端16pxへ半透明重畳する。
- ゲーム画面内へ複数のRA矩形を積み重ねない。

### 2.2 一時通知

- 表示は常に1件だけとする。
- 保存済み`Notification duration`の3/5/8秒を使用する。
- 既存のCritical、Important、Normal、Low、同優先度FIFO、高優先度による中断、残り時間からの
  再開、64件上限を維持する。
- 実績解除、Game/Subset completed、Leaderboard結果、切断・復旧、server error等の既存通知を
  対象とする。Rich Presence変更は通知へ戻さない。
- 実績解除は16px badge、タイトル、ポイントを行幅内で表示し、超過部分は末尾省略する。
  自動横scrollしない。Hardcore状態labelはPhase 8で実際のHardcoreセッションを有効化した後に
  追加し、Softcore固定のPhase 6では表示しない。
- 通知は継続pageより常に優先する。通知中もpage状態と選択位置を保持し、切替時間を消費しない。
- フルオーバーレイ中は通知を隠し、既存どおり寿命を消費しない。

### 2.3 状態page

- Challengeはachievement ID、trackerはtracker IDをkeyとしてSHOW・UPDATE・HIDEを保持する。
- Progressはゲーム全体で1件を保持し、SHOW・UPDATE・HIDEを反映する。
- 各有効Challenge、Progress、trackerを1pageとする。
- 0pageでは通常ステータス、1pageでは固定表示、2page以上では3秒ごとに次pageを表示する。
- 切替は瞬時とし、fade、slide、横scrollを行わない。
- page順はChallengeのSHOW到着順、Progress、trackerのSHOW到着順とする。Challengeとtrackerの
  同種内はSHOW到着順とする。
- SHOWは追加pageを直ちに選択してtimerを再開する。UPDATEは内容だけ更新し、選択とtimerを
  変更しない。HIDEで選択pageが消えた場合は次の有効pageへ移る。
- 文頭は`RA <現在位置>/<総数> Challenge:`、`RA <現在位置>/<総数> Progress:`、または
  `RA <現在位置>/<総数> Leaderboard:`とする。
- ChallengeとProgressは16x16 badge領域を常に確保し、placeholderでも文字位置を変えない。
- Progressは`measured_progress`を優先し、空の場合は`measured_percent`を表示する。
- 同一実績のChallenge HIDEとAchievement Triggeredを同じframeで受けた場合、HIDEを先に反映して
  から解除通知を表示する。
- trackerと別実績の解除、別Challenge、複数trackerは同時に内部保持できるが、画面には選択中の
  1pageだけを表示する。

### 2.4 Progress固有の寿命

- Progress IndicatorはRich Presenceではない。measured条件を持つ実績の進捗変化を短時間示す。
- `rc_client`は複数実績から解除に最も近い1件だけをProgress対象として選ぶ。
- `rc_client`は進捗変化のたびにSHOWまたはUPDATEを出し、最後の変化から2秒後にHIDEを出す。
- SHOWはProgress pageを直ちに選択する。UPDATEは値だけ更新し、XM8の3秒切替timerをリセットしない。
- UPDATEが別のachievementを指す場合は、単一Progress pageのachievement ID、title、badge、
  `measured_progress`、`measured_percent`をまとめて置き換える。
- HIDEは即時にpageを削除する。通知やフルオーバーレイに隠れている間にHIDEされたProgressを
  待機queueへ移したり、後から再表示したりしない。
- Progressの2秒寿命は`rc_client`が管理する。XM8のNotification duration 3/5/8秒を適用しない。
- 実績解除通知はProgressより優先する。解除通知中にProgressがHIDEされた場合、通知終了後は
  Challengeまたはtrackerへ戻る。

### 2.5 手動切替と入力

- `Status area`有効、通知なし、フルオーバーレイなし、有効pageが2件以上の場合だけ手動切替を
  有効にする。
- mouseは左button、touchは1tapを使用する。pointer downとupが同じステータス行内で成立し、
  dragしていない場合だけ次pageへ進む。
- 手動切替後は3秒timerを再開する。
- 通知をtapしてもdismissやpage切替を行わない。
- `Status area`無効時は、重畳されたRA行のmouse／touchを一切消費せずVMへ渡す。
- keyboard／controllerへ専用の手動切替操作は追加しない。

### 2.6 対象外

- 独立した右上Challenge、左上tracker、右下toast。
- 複数通知または複数継続pageの同時描画。
- ゲーム中の文字scroll、fade、slide。
- 通知履歴画面とゲーム終了後の送信エラー履歴。これは既存の別残件として維持する。

## 3. 実装責務

### 3.1 `RaOverlay`

- 通知の同時active数を3から1へ変更する。
- Challenge／Progress／trackerのpage model、安定したSHOW順、現在index、3秒timerを追加する。
- SHOW・UPDATE・HIDE、manual next、pause/resume、tick wrapをSDL描画から独立したAPIにする。
- 描画側へ「通知1件」または「継続page 1件」の合成済みsnapshotを返す。
- `Clear()`とゲームセッション停止で通知、page、timer、pointer候補をすべて破棄する。

### 3.2 `RaService`と`App`

- deep copy済みRA eventを到着順に`RaOverlay`へ渡す。
- Challenge SHOW/HIDE、Progress SHOW/HIDE/UPDATE、tracker SHOW/HIDE/UPDATEを一時通知へ変換しない。
- Progressの`measured_progress`、`measured_percent`、badge、titleをdeep copy済みeventから保持する。
- Achievement Triggeredと同じframeのChallenge HIDE順を保持する。
- 通知、page切替、HIDE、badge取得完了で`Video::DrawCtrl()`を要求する。
- `Status area`有効時だけstatus hit testを行い、down/up一致とdrag抑止後にmanual nextを呼ぶ。
- フルオーバーレイ表示中は通知とpage切替timerをpauseし、閉じた時点から再開する。

### 3.3 `Video`

- RA固有のevent、ID、timer、priorityを保持しない。
- Appから渡されたRA行snapshotがある場合だけ、通常ステータスの代わりにステータスbuffer全幅を
  描画する最小接続APIを追加する。
- RA行が消えたframeで通常ステータス全項目を強制再描画する。
- `Status area`有効時の論理y=400..417をgame inputと区別できるhit testを提供する。
- `Status area`無効時は論理y=384..399のRA表示をpointer targetとして扱わない。

## 4. 実装順序

1. 規範仕様と本計画を基準線としてcommitする。
2. `RaOverlay`へChallenge／Progress／trackerのpage状態機械と単一通知選択を実装する。
3. `RaService` eventからpage状態への接続と、同一frame event順序を実装する。
4. `Video`へ全幅動的ステータス行の描画・通常表示復元を追加する。
5. mouse／touchのstatus hit testとmanual nextを追加する。
6. badge、長文clip、Status area ON/OFF、各倍率を視覚検証する。
7. RA ON/OFF、Normal回帰、sanitizer、ASCEND手動受入を完了して実施記録を更新する。

状態機械、描画、入力を同時に変更せず、各段階で自動testを通してから次へ進む。

## 5. 自動テスト

### 5.1 通知queue

- 同時activeが常に1件で、2件目以降が待機する。
- Criticalが表示中Lowを中断し、Critical終了後に残り時間から再開する。
- 同優先度FIFO、64件上限、空通知拒否、duration 0拒否。
- pause中に寿命を消費せず、SDL tick 32bit wrap境界でも期限切れする。

### 5.2 page状態機械

- 0件、1件、複数件の表示。
- Challenge SHOW順、単一Progress、tracker SHOW順の順序。
- SHOWで当該pageを選択、UPDATEで選択とtimer不変、HIDEで安全に次pageへ移動。
- 3秒直前・ちょうど・直後の切替とtick wrap。
- manual nextとtimer再開。
- 通知中およびフルオーバーレイ中のtimer停止と復帰。
- Challenge HIDEの後にAchievement Triggeredを処理し、解除済みpageが復活しない。
- tracker表示中に別実績を解除し、通知終了後にtrackerへ戻る。
- 複数tracker更新時もUPDATEだけで選択pageが固定されないこと。
- Progress SHOWで即時選択し、UPDATEで表示値だけが変わり、HIDEで即時削除されること。
- Progress UPDATEで3秒切替timerがリセットされないこと。
- 通知中またはフルオーバーレイ中にProgress HIDEを受けた場合、復帰後に再表示しないこと。
- `measured_progress`が空の場合に`measured_percent`へfallbackすること。
- session stopと`Clear()`で全状態を破棄すること。

### 5.3 描画と入力

- RA行が640x16内だけを書き換え、VM frame領域外へ書かない。
- RA表示中に通常status文字が残らず、終了後に全項目が復元される。
- 最長title、Shift-JIS非対応文字、badge未取得・破損でもoverflowとlayout移動がない。
- `Status area`有効時だけdown/up一致tapでmanual nextする。
- down/up不一致、drag、通知中、page 1件、フルオーバーレイ中はmanual nextしない。
- `Status area`無効時のmouse／touchがRAで消費されず、既存VM入力へ到達する。
- RA OFF buildでは新しいevent処理、描画、入力分岐がcompile・linkされない。

## 6. ビルド・回帰検証

変更後に少なくとも次を実行する。

```sh
cmake --build build-ra
ctest --test-dir build-ra --output-on-failure
cmake --build build-normal
ctest --test-dir build-normal --output-on-failure
```

実際のbuild directory名が異なる場合は現在のCMake preset／既存実施記録に合わせ、実行commandと
結果を実施記録へ残す。`ra_overlay_test`はAddressSanitizerとUndefinedBehaviorSanitizerを有効にした
構成でも実行する。

## 7. 手動受入

テスト用D88は次を使用する。

```text
/Volumes/CrucialX6/roms/PC88/TEST/ascend.d88
```

### 7.1 640x400、Status area ON

1. 通常時にディスク、fps、NOWAIT、mode/clockが従来どおり表示される。
2. Challenge開始時に通常status全体が消え、Challenge 1pageだけが表示される。
3. ProgressとLeaderboard trackerも有効にし、3秒ごとに有効pageが瞬時に切り替わる。
4. ステータス行をclick／tapすると直ちに次pageへ移り、約3秒後に自動切替が再開する。
5. Progressやtrackerの値が頻繁に更新されても、自動page順と3秒周期がリセットされない。
6. Progressは進捗変化時に表示され、最後の変化から約2秒後のHIDEで消える。
7. 実績を解除すると解除通知だけが全幅表示され、終了後に有効なpageへ戻る。
8. 同じ実績のChallengeが解除された場合、通知後にそのChallengeが再表示されない。
9. すべてHIDEされた時点で通常status全体が欠けずに復元される。

### 7.2 Status area OFF

1. RA表示がない間は下端にRA行が残らない。
2. 通知または継続pageがある間だけ下端16pxへ半透明表示される。
3. 下端をmouse／touch操作してもpageは切り替わらず、PC-88側へ入力が届く。
4. RA表示終了後にゲーム画面下端が欠けずに再描画される。

### 7.3 倍率と競合

- macOSの640x400、960x600、1280x800以上で文字、badge、枠のclipを確認する。
- softkey表示、RAフルオーバーレイ開閉、menu開閉、静止画面、Full Speedで消去漏れを確認する。
- 複数通知、複数Challenge、Progress、複数trackerを発生させても、RA行が1本を超えないことを確認する。

## 8. 完了条件

- [03_オーバーレイUI仕様.md](03_オーバーレイUI仕様.md)、本計画、実装、テストが一致する。
- ゲーム画面内に従来の右下toast、右上Challenge、左上trackerが残っていない。
- RA表示中はステータス行全体を使用し、同時表示は常に1件である。
- Challenge／Progress／trackerの3秒自動切替と、Status area有効時のclick／tap切替が機能する。
- ProgressのSHOW／UPDATE／約2秒後のHIDEを尊重し、古いProgressを再表示しない。
- 通知による中断後に継続pageが正しく復帰する。
- Status area無効時のRA表示がVM pointer入力を奪わない。
- RA ON/OFFの全自動test、sanitizer、Normal回帰が成功する。
- ASCENDを使った手動受入結果とscreenshotを後続のPhase 6実施記録へ残す。
