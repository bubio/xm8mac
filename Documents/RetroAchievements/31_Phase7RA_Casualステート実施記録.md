# Phase 7 RA Casualステート実施記録

## 1. 用語と目的

2026-07-20、Phase 7のmacOS RA Casualセーブステート対応を実装した。RetroAchievementsの
現行利用者向け名称に合わせて「Casual」と呼ぶ。`rcheevos` APIや既存DB定数に残る
`Softcore`は内部互換名であり、別の動作モードではない。

Casualでセーブステートが許可されること自体は従来と変わらない。本対応の目的は、XM8のVM状態
だけを過去へ戻してRAの条件hit数、Challenge、Progress、Leaderboard trackerが未来のまま残る
不整合を防ぐことである。利用者向けの新しい設定は追加せず、NormalとRAで同じ10 slotの操作体系を
使うが、保存先の違いが明確になるようメニュー導線を分離する。

## 2. 実装範囲

- Normal、RA Casual、RA Offlineの同一slotを別fileへ保存する。
- Casual stateへ`rc_client_serialize_progress_sized()`の結果を格納し、load時に
  `rc_client_deserialize_progress_sized()`で復元する。
- XM8 state bodyの後ろへ`XMRA` chunkと`XMRF`固定footerを追加する。
- footer、chunk size、予約byte、body size、body CRC32、chunk CRC32、mode、RA Game ID、
  anchor媒体MD5、rcheevos versionをVMへ適用する前に検証する。
- RA stateは一時fileからrenameして確定し、途中失敗で既存slotを壊さない。
- VM body load前に現在状態を一時保存し、body load失敗時は直前状態へ戻す。
- VM bodyが成功し、RA progress復元だけが失敗した場合はVMを維持し、RA runtimeをresetして
  通知する。
- RA専用load中のVM再構築だけは現在のRAセッションを保持する。Normal loadは従来どおり
  RAを再識別する。
- RA mode有効時は起動時`Load(0)`と終了時`Save(0)`を呼ばず、Normalの自動stateと混在させない。
- RA mode中のslot 0は手動slotなので、Save／Load一覧から`(AUTO)`表記を外す。
- RA mode OFFではMain menuにNormal用Load／Saveを表示し、RA mode ONではこれを隠す。
- RA mode ONではRetroAchievements menu内にRA専用Load／Saveを表示し、slot画面titleと戻り先も
  RA専用にする。
- 識別中、ゲーム未開始、RA管理媒体なしではRA専用項目の選択時に理由を通知し、slot画面を開かない。

保存先は次のとおり。

```text
Normal:     <setting dir>/state<slot>.bin
RA Casual: <RA root>/states/<RA Game ID>/<anchor MD5>/state<slot>.bin
RA Offline:<RA root>/states/offline/<anchor MD5>/state<slot>.bin
```

## 3. 安全上の境界

RA stateのanchor MD5は、RA Libraryが管理する
`ra/media/<anchor MD5>/working.d88`のdirectory名から取得する。実行中に書込み可能なworking copy
全体を再hashして原本MD5と比較してはいけない、という既存媒体仕様を維持する。Library管理外の
媒体、RA識別開始中、ゲーム未ロード時はRA state操作を開始しない。

破損metadata、別ゲーム、別媒体、別mode、異なるrcheevos versionは、Setting、DiskManager、
TapeManager、VMへ適用する前に拒否する。RA chunkのない既存Normal stateもRA modeでは拒否するが、
Normal modeでは従来の形式として読める。

## 4. 自動試験

追加した`ra_state_store_test`は次を検証する。

- Casual／Offline round tripと保存先分離。
- footer欠落、footer size不一致、未知chunk version。
- body CRC、chunk／progress CRC、予約byteの破損。
- chunkのない旧stateの拒否。
- Game ID、anchor MD5、rcheevos version不一致。
- Casualのprogress欠落、Offlineへのprogress混入、不正MD5。
- directory作成を含むatomic file round trip。
- slot範囲、未知mode、最大file size、既存slotの完全置換。

`fileio_error_test`は、正常read/write、短いreadの異常保持、再open時の異常状態clearを検証する。

`ra_service_test`へ、実ゲーム定義をロードした`rc_client` progressのserialize、破損progress拒否、
正常progress再復元を追加した。

2026-07-20の結果:

```text
RA ON Debug build + ctest:  22/22 passed
RA OFF Debug build + ctest: 10/10 passed
ASan/UBSan RA ON全試験:     22/22 passed
git diff --check:            passed
```

macOSの現行ASanはLeakSanitizerを提供しないため、`detect_leaks=0`とし、AddressSanitizerと
UndefinedBehaviorSanitizerは全対象で有効にした。

## 5. 手動受入

テスト媒体は`/Volumes/CrucialX6/roms/PC88/TEST/ascend.d88`を使う。

1. `build-ra/xm8.app`を起動し、RA LibraryからASCENDを開始して`RA: identified`後まで待つ。
2. ゲーム中の任意の状態をslot 1へSaveする。
3. スコアと画面状態を進めた後、slot 1をLoadする。
4. ゲーム画面とスコアが保存地点へ戻り、Load直後に実績の誤解除や古いtrackerの残留がないことを
   確認する。
5. 同じslot 1へ再Saveし、Load一覧に時刻が表示され、再Loadできることを確認する。
6. RA modeを有効にしたままアプリを終了・再起動し、Normal用slot 0の自動復元が行われないことを
   確認する。
7. RA mode OFFではMain menuにLoad／Saveがあり、RetroAchievements menuにはないことを確認する。
8. RA mode ONではMain menuからLoad／Saveが消え、RetroAchievements menuに表示されることを
   確認する。専用画面titleが`RA Load State`／`RA Save State`で、ESCでRA menuへ戻ることも確認する。

通常利用の受入で破損fileを手作業生成する必要はない。破損、別game、別media、別versionは
自動試験で網羅する。

## 6. 完了判定と次回

- Phase 7実装: 完了
- RA ON／OFF回帰とsanitizer: 完了
- macOS実ゲーム手動受入: 完了（2026-07-20、利用者確認）
- Phase 7完了判定: 完了（2026-07-20）

利用者がASCENDでRA Casual stateの手動Save／Loadと、RA modeを有効にしたまま終了・再起動
した際にNormal用slot 0が自動復元されないことを確認した。

次はPhase 8のHardcore policyへ進む。Phase 8では
Save／Load、速度変更、疑似高速disk、debug操作などの全入口制限と、実セッションmode表示を扱う。
