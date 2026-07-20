# Phase 8 macOS Hardcore実施記録

## 1. 対象

2026-07-20、macOS基準実装へHardcoreの実効セッション制約を追加した。
テスト媒体は`/Volumes/CrucialX6/roms/PC88/TEST/ascend.d88`、確認用アプリは
`build-ra/xm8.app`とする。

実装、自動検証、実HTTPSを使う手動受入まで完了した。

## 2. 実装

### 2.1 中央セッションポリシー

`Source/RA/ra_session_policy.h`へ、設定上の`Casual／Hardcore`と実効状態
`Normal／Casual／Hardcore／Offline`を分離した純粋な判定を追加した。

- `Starting`、`Active`、`ActiveDisconnected`では選択したオンラインmodeを適用する。
- 通信切断だけではActive Hardcoreを弱めない。
- セッション開始失敗後の`Offline`ではHardcore表示と制限を残さない。
- RAを選択していてもゲームセッションのない`Ready`はNormal相当とする。

Save／Load、Full Speed、Pseudo fast disk、診断操作の可否はこの判定を共通利用する。
現在のAppはユーザー向けdebugger、breakpoint、frame step、memory write入口を公開していない。
RA用メモリAPIもread-onlyであるため、オンライン中に到達可能な診断書換え入口はない。

### 2.2 mode設定とrcheevos

RAメニューに`Hardcore` check行を追加した。公開設定は従来の`RA mode`と合わせて2つだけで、
選択値は既存`ra_settings.last_mode`へ保存する。

ゲームをロードする前に毎回、Casualは`rc_client_set_hardcore_enabled(..., 0)`、Hardcoreは
同APIへ`1`を明示する。ロード済みゲームで同APIが発生させるRESETイベントも取り込み、
次のVM frameより前にVM reset、続いて`rc_client_reset()`を1回だけ実行する。

Casual／Hardcore切替は現在のゲームをcold resetしてセッションを再開始する。Active Hardcoreから
Casualへ戻す場合は`End Hardcore Session?`確認画面を経由する。

### 2.3 操作制限

- Hardcore中はRA専用Load／Save行を表示しない。
- menuやshortcutが最終的に通る`App::Load()`、`App::Save()`でもHardcoreを拒否する。
- `App::FullSpeed()`で拒否するため、Main menuとAlt+F11の双方を迂回できない。
- オンラインCasual／Hardcore開始時はPseudo fast diskを一時OFFにし、実行中の変更を拒否する。
- Offline移行、RA終了、RA無効化時に開始前のPseudo fast disk値を復元する。
- システム／clock変更時はVMを再構築した後、マウント済みDrive 1からRAセッションを再開始する。
- 別ゲームD88を開く場合は前ゲームのlive memoryを引き継がずcold resetする。

### 2.4 Reset、menu、overlay、background

- 通常ResetはVMを先にresetし、同じロード済みRAゲームに対して`rc_client_reset()`を1回呼ぶ。
- Hardcore中のXM8設定menuはゲーム入力だけを遮断し、VM、音声、RA frame評価を止めない。
- Library、Achievements、LeaderboardsもHardcore中は非停止overlayとする。
- Loginは文字入力を所有するため従来どおり停止型とする。Active HardcoreからLoginへ進む通常導線はない。
- macOS backgroundではVMと音声を止め、最大1秒間隔のtimeoutでHTTP drainと`rc_client_idle()`を
  継続する。foreground復帰時は時間差frameを追いつき実行せず、通常timingを再確立する。
- `RaService::CanPause()`は`rc_client_can_pause()`と`frames_remaining`を公開し、自動試験で
  初回Hardcore pauseとCasual常時許可を確認する。Hardcoreのmenu／閲覧overlayはpause自体を
  発生させない。

### 2.5 表示

実績解除通知は実効Hardcoreセッションの場合だけ`[Hardcore]`を付ける。Offlineへ移行した後や、
設定としてHardcoreを選択しただけのReady状態では付けない。

## 3. 自動検証

`ra_session_policy_test`を追加し、Normal、Casual、Hardcore、切断継続、Offlineの操作matrixを検証した。
`ra_service_test`には次を追加した。

- service作成直後をCasualへ明示すること。
- 未ロード状態でHardcoreを設定でき、RESET待ちにならないこと。
- Hardcore start session requestが`h=1`を含むこと。
- ロード済み状態でHardcoreを有効化するとRESETイベントが1件だけ発生すること。
- `CanPause()`の初回Hardcore許可とCasual許可。

結果:

```text
RA ON Debug build + ctest:  23/23 passed
RA OFF Debug build + ctest: 10/10 passed
ASan/UBSan RA ON全試験:     23/23 passed
git diff --check:            passed
ASCEND SHA-256 before/after:  782a1ed5...fc4ea50（一致）
```

SanitizerはmacOSで利用できないLeakSanitizerだけ`detect_leaks=0`とし、AddressSanitizerと
UndefinedBehaviorSanitizerをアプリと全testへ適用した。
`ra_inspect_media`でもASCENDを再検査し、media MD5とbank 0のRA hashがともに
`5def00835e061a54fe6d1fa2d5a8d2b0`、bank名が`ASCEND`であることを確認した。

## 4. 手動受入

自動試験では代替できない実HTTPS、描画、実VM継続だけを確認する。

1. `build-ra/xm8.app`を起動し、RA LibraryからASCENDを開始して識別完了まで待つ。
2. RAメニューで`Hardcore`をONにする。ASCENDがcold resetされ、再識別後もHardcoreがcheck済みで、
   RA Load State／Save Stateが表示されないことを確認する。
3. ゲームへ戻りAlt+F11を押す。`Full Speed is unavailable in Hardcore`通知が出て、速度が変わらない
   ことを確認する。
4. Hardcore中にF11でmenuを開いたまま数秒待ち、閉じた後にASCENDのTIMEが進んでいることを確認する。
   AchievementsまたはLeaderboards画面でも同じようにゲームが進むことを確認する。
5. Hardcore中にSystem Optionsの`Pseudo fast disk access`を選び、ONにならず拒否通知が出ることを
   確認する。
6. Resetを実行し、ASCENDはresetされるがRAが同じゲームとして再びActiveになることを確認する。
7. RAメニューの`Hardcore`をOFFにし、確認画面でいったん`No`を選ぶとHardcoreが継続すること、
   再度OFFにして`Yes (Switch to Casual)`を選ぶとcold reset後にRA Load／Saveが再表示されることを
   確認する。
8. アプリを終了・再起動し、最後に選択したCasual／Hardcoreが保持されていることを確認する。

Pseudo fast diskの値復元を確認する場合は、RA mode OFFで先にONにし、RA mode ONで強制OFF、
RA mode OFFへ戻した時にONへ復元されることを追加確認する。

2026-07-20、利用者が上記のHardcore開始、禁止操作、非停止UI、Reset、Casual復帰、
mode永続化をASCENDで確認し、問題なしとして受入完了した。

## 5. 完了判定

- Phase 8実装: 完了
- RA ON／OFF回帰とsanitizer: 完了
- macOS実HTTPS／ASCEND手動受入: 完了（2026-07-20、利用者確認）
- Phase 8完了判定: 完了（2026-07-20）

次はPhase 9のWindows移植へ進む。最初にVisual Studio project構成、Windows HTTP／資格情報保存、
設定pathと既存Windows build可否を再監査し、macOSで確定した共通仕様を変更せず移植範囲を確定する。
