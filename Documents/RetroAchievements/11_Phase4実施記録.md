# Phase 4実施記録: macOS HTTP・認証

## 1. 現在の完了範囲

Phase 4として、macOS HTTP adapter、`rc_client`認証サービス層、
ハッシュ指定のゲームロード開始、保存済みtokenによるアプリ内セッション開始、
トップ階層のRetroAchievementsメニューからのRAモード操作、最小通知表示を追加した。

- `RaHttpClient`契約を追加した。
- fake HTTP backendを追加した。
- 保存tokenの保存、読込、削除を追加した。当時の`credentials.bin`保存は現在廃止済み。
- `rc_client_server_call_t`から`RaHttpClient`へ変換するbridgeを追加した。
- macOS用`NSURLSession` backendを追加した。
- `RaService`を追加し、`rc_client`所有、password login、保存token login、logout、
  shutdown順序を接続した。
- password login成功時はRAから返されたtokenを保存する。
- 保存token login失敗時は保存tokenを削除する。
- logout時は`rc_client_logout()`後に保存tokenを削除する。
- Phase 4初期実装ではD88全体MD5を`rc_client_begin_load_game()`へ渡してゲーム識別、
  実績定義取得、RA session開始を`rc_client`へ委譲していた。Phase 5中にRAの
  Supported Game Hashes実運用に合わせ、マルチイメージD88では起動bank単位の
  RA識別hashを渡す方式へ修正した。
- ゲームロード成功時は`rc_client_get_game_info()`からGame ID、console ID、title、
  hash、badge URLをsnapshotへ反映する。
- ゲームロード失敗時は当該起動セッションのRAを
  `DisabledForSession`として固定し、`rc_client_unload_game()`する。
- `rc_client_set_event_handler()`を接続し、rcheevosイベントをXM8側で保持できる
  所有コピーのイベントqueueへ変換する。
- `DoFrame()`と`Idle()`を追加し、`rc_client_do_frame()`、
  `rc_client_idle()`を`RaService`経由で呼べるようにした。
- `rc_client_set_allow_background_memory_reads(client, 0)`を設定し、RAメモリ読み出しを
  明示的なframe/idle処理内に限定した。
- `RaService`にVM側メモリ読み出し用のuserdata付きcallbackを追加した。
- macOS RA ON buildの`App`へ`RaService`生成を接続した。
- User-AgentへXM8 versionとrcheevos versionを含めるようにした。
- RAモードでDrive 1のD88を作業コピーへ解決した時点で、起動bankのRA識別hashを
  当該セッションのRA識別対象として固定するようにした。
- 保存済みtokenがある場合、アプリループで自動login、hash指定ゲームロード、
  frame/idle処理まで進行するようにした。
- 保存済みtokenがない、login失敗、ゲームロード失敗の場合はRAを当該セッションで
  無効化し、D88起動自体は継続する。
- トップ階層へRetroAchievementsメニューを追加し、RA mode、RA status、保存token login、
  logoutを配置した。
- RA mode ON/OFFは`ra_settings`へ保存する。
- RA mode OFF時は現在のRAゲームセッションを破棄するが、保存済みtokenは維持する。
  token削除は明示的なlogout時だけ行う。
- RA状態、login要求、ゲーム識別、ゲームロード、代表的なRAイベントを
  エミュレーション画面上の短い通知として表示する。
- RA ON buildだけへ上記を組み込み、Normal buildには組み込んでいない。

実RAサーバーへのHTTPS疎通はmacOS backendとして実装済みだが、認証情報を使う手動疎通試験は
未実施である。オーバーレイpassword login UI、実績一覧、バッジ画像、Leaderboard一覧は
Phase 5以降で実装する。

## 2. 認証情報ファイル

`Source/RA/ra_credentials.*`を追加した。当時は`credentials.bin`へusername/tokenを
保存する仕様だったが、現在は廃止済みである。

現在の実装規則:

- tokenは`RetroAchivementsIntegrationHandbook/docs/credential-storage.md`に従い、
  OSのcredential storeへ保存する。
- usernameは保存済みtoken loginのaccount hintとして通常ファイルへ保存する。
- password、token、POST body、認証付きURLをログや通常ファイルへ保存しない。
- logout相当の削除APIを持つ。
- メモリ上のtokenを上書きしてから破棄するAPIを持つ。
- 保存token loginが資格情報拒否になった場合はcredential storeからtokenを削除する。

## 3. HTTP契約

`Source/RA/ra_http_client.h`を追加した。

主な契約:

- `post_data == NULL`相当は`has_post_data == false`で表す。
- 空POSTは`has_post_data == true`かつ`post_data.empty()`で表し、GETへ変換しない。
- requestは`Send()`時点で所有コピーとして扱う。
- 完了応答は`DrainCompleted()`で取り出す。
- timeout既定値はconnect 10秒、total 30秒。
- response上限既定値は8MiB。

`Source/RA/ra_http_fake.*`はテスト用backendであり、実通信は行わない。cancel済みrequestの
遅延responseは破棄する。

`Source/RA/ra_rc_client_http.*`は`rc_client_server_call_t` adapterである。

- `rc_api_request_t::url`、`post_data`、`content_type`を`Send()`前に所有コピーへ変換する。
- `RaService`導入後は`rc_client_get_userdata(client)`を`RaService`本体に割り当て、
  `RaService::ServerCall()`からbridgeへ委譲する。
- bridge単体の`ServerCall()`もテスト用・直接利用用として残す。
- bridge未設定または不正requestでは同期的に
  `RC_API_SERVER_RESPONSE_CLIENT_ERROR`を返す。
- transport successではHTTP statusとbodyをそのまま`rc_api_server_response_t`へ渡す。
- timeoutとretryable transport errorは
  `RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR`へ変換する。
- cancel、oversize、非retryable transport errorは
  `RC_API_SERVER_RESPONSE_CLIENT_ERROR`へ変換する。
- bridge generationが進んだ後に届いた古い完了応答はpendingから除去するが、
  `rc_client` callbackへは渡さない。
- callback中だけ有効なbody pointerを渡し、callback後は保持しない。

`Source/RA/ra_http_mac.*`はmacOS専用`NSURLSession` backendである。

- GET、空POST、通常POSTを区別する。
- RA API requestではredirectを追跡しない。
- 画像requestだけHTTPSからHTTPSへのredirectを最大5回許可する。
- response上限超過時はtaskをcancelし、`Oversize`として完了queueへ積む。
- `NSURLSession` callbackは完了queueへ積むだけで、VMやUIを操作しない。
- Cookieは使用しない。
- User-Agentをrequest headerへ設定する。

## 4. RAサービス層

`Source/RA/ra_service.*`を追加した。

責務:

- `rc_client_t`を生成、所有、破棄する。
- `rc_client_set_userdata()`へ`RaService`本体を接続し、HTTP server callとevent handlerの
  両方を`RaService`経由で処理する。
- 初期状態では`rc_client_set_hardcore_enabled(client, 0)`によりSoftcore相当にする。
- `rc_client_set_allow_background_memory_reads(client, 0)`により、RAメモリ読み出しを
  `DoFrame()`または`Idle()`中へ限定する。
- アプリ統合時は`RaHostReadMemoryFunc`からVMの
  `read_ra_inspection_memory()`を呼び、`rc_client` userdataは`RaService`本体のままにする。
- password loginを`rc_client_begin_login_with_password()`へ渡す。
- 保存済みtoken loginをcredential storeからの読込後に
  `rc_client_begin_login_with_token()`へ渡す。
- HTTP完了は`DrainHttp()`からbridgeをdrainし、`rc_client` callbackをメイン側で実行する。
- password login成功時は、`rc_client_get_user_info()`から取得したusername/tokenを保存する。
- 保存済みtoken login失敗時は保存tokenを削除し、当該tokenを再利用しない。
- logout時は`rc_client_logout()`を呼び、保存tokenも削除する。
- shutdown時は`CancelAll()`、drain、`rc_client_destroy()`、bridge破棄の順で処理する。
- `BeginLoadGameByHash()`で32文字MD5 hexだけを受け付ける。
- load中の二重load要求は拒否する。
- load開始前にHTTP bridge generationを進め、前セッションの遅延HTTP完了を破棄する。
- `rc_client_begin_load_game()`へhashを渡し、`achievementsets`、`startsession`等のRA API
  詳細は`rc_client`に委譲する。
- load成功時は`RaGameSessionSnapshot`を`Loaded`へ更新する。
- load失敗時は`RaGameSessionSnapshot`を`DisabledForSession`へ更新し、
  途中再接続によるRA復帰を行わない。
- `UnloadGame()`は`rc_client_unload_game()`とbridge generation更新を行い、
  game snapshotを初期化する。
- `DoFrame()`はゲームが`Loaded`のときだけ`rc_client_do_frame()`を呼ぶ。
- `Idle()`はログイン・ロード状態に関係なく、サービス準備済みなら`rc_client_idle()`を呼ぶ。
- `IsProcessingRequired()`で`rc_client_is_processing_required()`を参照できる。
- `TakeEvents()`で蓄積済みイベントを取得し、取得後はqueueを空にする。
- rcheevosのevent callbackで渡されるポインタはcallback中だけ有効なため、
  実績、Leaderboard、scoreboard、server error、subsetの文字列と数値を即時コピーする。
- Rich Presenceは`DoFrame()`/`Idle()`後に前回値と比較し、変化時だけ
  `RichPresenceChanged`イベントを積む。

Phase 4では、イベントqueueのうち実績解除、Leaderboard開始/失敗/送信/結果、
Game Completed、server error、disconnect/reconnect、subset completed、Rich Presence変更を
短い通知へ変換している。Challenge Indicatorや進捗indicatorの常駐表示はPhase 5の
本格オーバーレイで扱う。

## 5. アプリ統合

RA ON buildの`Source/UI/app.*`と`Source/UI/menu.*`にmacOS向けの最小統合を追加した。

- `RaLibrary`と`RaMediaStore`初期化後、VM生成後に`RaService`を生成する。
- HTTP backendはmacOS `NSURLSession` adapterを使用する。
- RA service生成失敗時はRAモードだけを無効化し、Normal起動は継続する。
- `ResolveDiskForRaMode()`で作業コピーへ差し替えたDrive 1媒体/bankのRA識別hashを
  `ra_pending_game_hash`へ保持する。
- Drive 2や後続媒体の挿入だけではRA active mediaを変更しない。
- メニュー、バックグラウンド、power down中は`RaService::Idle()`を進める。
- VMがフレームを進めた場合は、実行フレーム数ぶん`RaService::DoFrame()`を試し、
  game未ロード中は`Idle()`へフォールバックする。
- Phase 4時点ではpassword入力UIがないため、保存済みtoken loginだけを自動実行する。
- `RaService::TakeEvents()`から取得した代表イベントを通知表示へ接続する。
- RA通知はVM描画後、`video->Draw()`前に既存`Font`でフレームバッファへ合成する。
- RetroAchievementsメニューでRA modeを切り替えられる。ON時は必要に応じて`RaService`を生成し、
  OFF時はRAゲームセッションだけをunloadする。
- RetroAchievementsメニューで保存token loginを明示的に再試行できる。
- RetroAchievementsメニューでRA statusを更新表示できる。

制約:

- Phase 4時点ではpassword入力UIを持たない。初回password loginは`RaService` APIと
  自動テストで検証済みであり、ユーザー入力はPhase 5のSDLオーバーレイlogin画面で実装する。
- 通知表示は短い状態表示のみで、実績一覧、バッジ、Leaderboard詳細、Challenge Indicatorの
  レイアウトはPhase 5へ送る。

## 6. テスト

`Tests/ra_credentials_http_test.cpp`と`Tests/ra_service_test.cpp`を追加した。

検証内容:

- credentials保存、読込、削除。
- Unix系で0600作成されること。
- credentialsの不正lengthを拒否すること。
- oversized username/tokenを拒否すること。
- token clear APIでメモリ上のtokenが空になること。
- HTTP fake backendがGET、空POST、通常POSTを区別すること。
- request bodyが所有コピーになっていること。
- cancel後の遅延responseが二重通知されないこと。
- `rc_client_server_call_t` adapterが空POSTを保持すること。
- adapterがtransport結果を`rc_api_server_response_t`へ分類すること。
- adapterが`rc_client` userdataからbridgeを取得すること。
- bridge generation更新後に届いた古いHTTP完了をcallbackせず破棄すること。
- macOS `NSURLSession` backendを生成し、空drainとcancel allができること。
- `RaService`がpassword loginを`rc_client`へ渡し、成功時に返却tokenを保存すること。
- `RaService`が保存token loginを`rc_client`へ渡し、password fieldを送信しないこと。
- 保存token拒否時にcredential storeからtokenを削除すること。
- logout時にcredential storeからtokenを削除すること。
- `BeginLoadGameByHash()`がRA識別hashを`achievementsets` requestへ渡すこと。
- `achievementsets`成功後に`startsession` requestが発行されること。
- load成功時にGame ID、console ID、title、hashがsnapshotへ反映されること。
- background memory read禁止下では、`startsession`応答後に`Idle()`を進めて
  load完了callbackが実行されること。
- 不正hashではHTTP requestを送信しないこと。
- load失敗時にRAを当該セッションで`DisabledForSession`に固定すること。
- game未load時の`DoFrame()`は`rc_client_do_frame()`を呼ばずfalseを返すこと。
- game未load時でも`Idle()`は呼び出し可能で、不要なイベントを生成しないこと。
- 実績イベントの文字列、ID、進捗値をrcheevos callback元ポインタから所有コピーへ
  変換すること。
- Leaderboard scoreboardイベントの上位entryを所有コピーへ変換すること。
- shutdown時にpending HTTPをcancel/drainし、pendingを残さないこと。
- RA serviceがuserdata付きhost memory callbackを使えることは、RA ON `xm8` buildで
  VM統合として確認する。
- RA ON `xm8` buildでトップ階層のRetroAchievementsメニューがコンパイルされること。
- Normal buildではRAメニュー項目、RA service、macOS HTTP backendが組み込まれないこと。

実行済み:

```text
cmake -S . -B build-ra -DXM8_ENABLE_RETROACHIEVEMENTS=ON
cmake --build build-ra --target xm8 ra_service_test ra_credentials_http_test
ctest --test-dir build-ra -R 'ra_service_test|ra_credentials_http_test|ra_library_store_test|ra_media_probe_test|ra_memory_map_test|ra_dependency_test|d88fixture_test|d88probe_test' --output-on-failure
cmake --build build --target xm8
ctest --test-dir build -R 'd88fixture_test|d88probe_test|clidisk_test' --output-on-failure
```

結果:

- RA ON: 8/8 passed
- Normal: 3/3 passed

## 7. Phase 4完了判定

Phase 4は完了扱いとする。

完了理由:

- macOS用HTTP backend、`rc_client` bridge、credentials、認証、ゲーム識別、session開始、
  frame/idle処理入口が揃っている。
- RAモード有効時だけD88作業コピー、保存token login、hash指定ゲームロードへ進む。
- 失敗時は当該セッションのRAを無効化し、ゲーム起動を継続する。
- トップ階層のRetroAchievementsメニューからRA mode ON/OFF、保存token login再試行、
  logout、状態確認ができる。
- 最低限の状態通知をSDLフレームバッファへ表示できる。
- RA ON buildとNormal buildの双方で回帰テストが通っている。

Phase 5へ送るもの:

1. SDLオーバーレイlogin UIからpassword loginを開始する。
2. Login文字入力と`SDL_TEXTINPUT`を接続する。
3. 実績一覧、実績詳細、Leaderboard一覧、Rich Presence表示を実装する。
4. `RaImageCache`とバッジ/アバター表示を接続する。
5. Challenge Indicator、進捗indicator、通知優先順位を本格実装する。
6. 実RAアカウントを使ったmacOS HTTPS手動疎通試験を実施する。

> 2026-07-18追記: PCは物理キーボード、AndroidはOS software keyboardを使う。
> RA独自キー配列は実装せず、Android実機で支障がある場合だけLogin専用
> native画面を検討する。
