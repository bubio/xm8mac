# Phase 5実施記録: オーバーレイ基盤

## 1. 現在の完了範囲

Phase 5の入口として、Phase 4で`App`へ直書きしていたRA通知状態を
`RaOverlay`へ分離した。

- `Source/RA/ra_overlay.*`を追加した。
- `RaOverlay`は通知本文、通知期限、Login画面の状態snapshotを保持する。
- `RaOverlay`はLogin画面の入力状態、focus、submit状態を保持する。
- 通知の実描画は従来どおり`App::DrawRaOverlay()`が既存`Font`、`Video`を使って行う。
- `RaOverlay`本体はSDL描画依存を持たないため、RA共通コードの単体テストから利用できる。
- Phase 4で実装した短いRA通知の見た目と表示位置は変更していない。
- トップ階層のRetroAchievementsメニューの`Login`から、物理キーボード入力用の
  Loginオーバーレイを開ける。
- RAメニューのLogin操作は、未ログイン時は`Login`、ログイン済み時は`Logout`として表示する。
- Logout後はRAメニューのLogin操作を即座に`Login`表示へ戻す。
- Login実行時にRA modeがOFFなら、RA modeをONにしてから処理する。
- RA modeがONの状態でアプリを起動した場合、保存済みtokenがあれば自動的にtoken loginを開始する。
- RA modeをOFFからONへ切り替えた場合も、保存済みtokenがあれば自動的にtoken loginを開始する。
- RAメニューの状態行は、ログイン済みでユーザー名が取得できている場合に
  `RA: logged in <user>`と表示する。
- RetroAchievementsメニューへ`Achievements`と`Leaderboards`を追加した。
- 独自デザインの中間RAホームオーバーレイは作らず、既存メニューをRAホーム相当として扱う。
- Achievementsは、ロード済みRAゲームの実績一覧を5件ずつ表示する最小オーバーレイを開く。
  RAゲーム未ロード時と実績なしの場合は画面内メッセージを表示する。
- Achievementsオーバーレイは選択枠、上下スクロール、選択項目の説明表示を持つ。
  キーボード、コントローラ、マウス、タッチで項目を選択でき、`Esc`または一覧外
  クリック/タッチで閉じる。
- Leaderboardsは一覧画面実装まで未実装通知を表示する。
- RAのSupported Game HashesはPC-88 D88をディスク単位で登録しているため、
  マルチイメージD88のファイル全体MD5をRA識別へ使う方針を修正した。
  保存管理用媒体IDは従来どおりD88ファイル全体MD5、RA識別hashは起動bankの
  D88イメージ範囲MD5とする。
- Loginオーバーレイは`SDL_TEXTINPUT`、Tab、Backspace、Enter、Escape、上下キーを扱う。
- LoginオーバーレイはUsername、Password、Login、Cancelへフォーカスを持ち、
  マウス、タッチ、方向キー、controllerでフォーカス移動と実行を行える。
- 入力欄フォーカス時は`SDL_StartTextInput()`、Login/Cancelなど入力欄以外への
  フォーカス移動またはLogin画面終了時は`SDL_StopTextInput()`を呼ぶ。
- AndroidではOSソフトウェアキーボードを第一候補とする。ただし実機表示確認は
  Android Phaseで行い、表示できない環境の保険としてRAオーバーレイ内入力手段を残す。
- Enterで`RaService::BeginLoginWithPassword()`を呼び、開始後にPasswordバッファを消去する。
- Login開始後はLogin画面をpending表示のまま維持し、成功時だけ閉じる。
- Login pending中は`Cancel`、`Esc`、Android Backを含むLogin画面操作を無効化する。
  `rc_client`の非同期login requestは中断せず、完了callbackを待つ。
- Login失敗時は短い通知だけを出し、RAから返った安全な詳細文または固定文言を
  Login画面内に表示する。Usernameは維持し、Passwordは消去する。
- マウスとタッチは押下開始targetと解放targetが一致した場合だけ`Login`または
  `Cancel`を実行する。解放位置だけで実行しない。

この段階では、ASCIIオンスクリーンキーボード、Library画面、実績詳細画面、
バッジ画像、Leaderboard画面はまだ実装していない。

## 2. 実装意図

Phase 5以降では、ログイン、ライブラリ、実績一覧、Leaderboardなどを
エミュレーション画面上のオーバーレイとして実装する必要がある。
RAの最上位入口は既存RetroAchievementsメニューを使う。

そのため、通知文字列や画面状態を`App`の一時変数として増やし続けず、
RA専用の状態オブジェクトへ集約する。描画自体は既存UI資産を使うため、
初期段階では`App`に残す。

`RaOverlay`へUI状態を集約することで、次の作業単位を分けられる。

- オーバーレイ画面状態遷移
- キーボード、コントローラ、マウス、タッチ入力
- ログインフォームとオンスクリーンキーボード
- 実績、Leaderboard、Rich Presence表示
- バッジ画像cacheとの接続

## 3. 受入条件

- RA ON buildで`RaOverlay`がコンパイルされる。
- Normal buildには`RaOverlay`を組み込まない。
- 既存のRA通知表示が維持される。
- 通知期限、空通知、snapshot保持の単体テストが通る。
- Login画面のfield切替、Password伏字、Submit、Cancel、Password消去の単体テストが通る。
- Login画面のポインタ操作、controller相当のfocus移動、失敗時のUsername維持と
  Password消去、Submit pending中の二重送信防止、pending中のCancel不可、
  pointer target判定の単体テストが通る。
- RetroAchievementsメニューにAchievementsとLeaderboardsが表示され、
  Achievementsは最小一覧オーバーレイ、Leaderboardsは未実装通知を出せる。
- 実績一覧snapshot、Achievementsオーバーレイの選択、スクロール、一覧外closeの
  単体テストが通る。
- Phase 4のRA service、media store、memory map関連テストが引き続き通る。

## 4. 検証結果

2026-07-02時点で次を確認した。

```sh
cmake -S . -B build-ra -DXM8_ENABLE_RETROACHIEVEMENTS=ON
cmake -S . -B build
cmake --build build-ra --target xm8 ra_overlay_test
cmake --build build --target xm8
ctest --test-dir build-ra -R 'ra_overlay_test|ra_service_test|ra_credentials_http_test|ra_library_store_test|ra_media_probe_test|ra_memory_map_test|ra_dependency_test|d88fixture_test|d88probe_test' --output-on-failure
ctest --test-dir build -R 'd88fixture_test|d88probe_test|clidisk_test' --output-on-failure
git diff --check
```

- RA ON build: 成功。
- Normal build: 成功。
- RA ON test: 9件成功。
- Normal test: 3件成功。
- whitespace check: 問題なし。

`font.cpp`のstring literal警告は既存警告であり、本Phaseの変更では扱わない。

## 5. 未完了項目

- ASCIIオンスクリーンキーボード。
- 既存softkeyはVMへのPC-88キー入力経路であり、RA Login文字列入力へ渡す変換層は未接続。
- RetroAchievementsメニューとLogin画面の視覚確認とスクリーンショット比較。
- Library画面とゲーム詳細画面。
- 実績一覧のスクロール、フィルタ、詳細表示。
- バッジ、アバター画像cacheの描画接続。
- Leaderboard一覧、送信結果、順位表示。
- Login以外のRA専用画面に対するタッチ操作とゲームコントローラ操作の同等化。
