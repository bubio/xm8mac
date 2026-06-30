# Phase 4実施記録: macOS HTTP・認証

## 1. 現在の完了範囲

Phase 4のうち、実RAサーバーへ接続しない共通土台、macOS HTTP adapter、
`rc_client`認証サービス層、ハッシュ指定のゲームロード開始を追加した。

- `RaHttpClient`契約を追加した。
- fake HTTP backendを追加した。
- `credentials.bin`の保存、読込、削除を追加した。
- `rc_client_server_call_t`から`RaHttpClient`へ変換するbridgeを追加した。
- macOS用`NSURLSession` backendを追加した。
- `RaService`を追加し、`rc_client`所有、password login、保存token login、logout、
  shutdown順序を接続した。
- password login成功時はRAから返されたtokenを`credentials.bin`へ保存する。
- 保存token login失敗時は`credentials.bin`を削除する。
- logout時は`rc_client_logout()`後に`credentials.bin`を削除する。
- D88全体MD5を`rc_client_begin_load_game()`へ渡してゲーム識別、実績定義取得、
  RA session開始を`rc_client`へ委譲する。
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
- RA ON buildだけへ上記を組み込み、Normal buildには組み込んでいない。

実RAサーバーへのHTTPS接続確認、実機D88からの自動ロード接続は未実装である。

## 2. 認証情報ファイル

`Source/RA/ra_credentials.*`を追加した。形式は
[02_ゲームライブラリとD88保存仕様.md](02_ゲームライブラリとD88保存仕様.md) の
`credentials.bin`仕様に従う。

```text
4 bytes  magic "XMR1"
4 bytes  little-endian format version (=1)
4 bytes  username UTF-8 byte length
4 bytes  token UTF-8 byte length
N bytes  username
M bytes  token
4 bytes  CRC32 of all preceding bytes
```

実装規則:

- usernameは256 byte以下、tokenは4096 byte以下だけ保存する。
- CRC不一致、不正長、未知versionは削除せず、load失敗として扱う。
- 保存は`credentials.bin.tmp`へ書き、成功後にatomic renameする。
- Unix系では一時ファイル作成時点から0600で作成する。
- logout相当の削除APIを持つ。
- メモリ上のtokenを上書きしてから破棄するAPIを持つ。

この実装は、RetroAchievementsIntegrationHandbookの「password/tokenをログへ出さない」
「保存tokenでのlogin失敗時はtokenを削除する」という方針と矛盾しない。ただし、保存先は
XM8の確定仕様どおりOS credential storeではなく`ra/credentials.bin`である。

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
- password loginを`rc_client_begin_login_with_password()`へ渡す。
- 保存済みtoken loginを`credentials.bin`読込後に
  `rc_client_begin_login_with_token()`へ渡す。
- HTTP完了は`DrainHttp()`からbridgeをdrainし、`rc_client` callbackをメイン側で実行する。
- password login成功時は、`rc_client_get_user_info()`から取得したusername/tokenを保存する。
- 保存済みtoken login失敗時は`credentials.bin`を削除し、当該tokenを再利用しない。
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

現時点では、イベントはサービス内部queueへ変換するだけで、SDLオーバーレイ表示には
まだ接続していない。

## 5. テスト

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
- macOS `NSURLSession` backendを生成し、空drainとcancel allができること。
- `RaService`がpassword loginを`rc_client`へ渡し、成功時に返却tokenを保存すること。
- `RaService`が保存token loginを`rc_client`へ渡し、password fieldを送信しないこと。
- 保存token拒否時に`credentials.bin`を削除すること。
- logout時に`credentials.bin`を削除すること。
- `BeginLoadGameByHash()`がD88全体MD5を`achievementsets` requestへ渡すこと。
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

実行済み:

```text
cmake -S . -B build-ra -DXM8_ENABLE_RETROACHIEVEMENTS=ON
cmake --build build-ra --target ra_service_test ra_credentials_http_test xm8
ctest --test-dir build-ra -R 'ra_service_test|ra_credentials_http_test|ra_library_store_test|ra_media_probe_test|ra_memory_map_test|ra_dependency_test|d88fixture_test|d88probe_test' --output-on-failure
cmake --build build --target xm8
ctest --test-dir build -R 'd88fixture_test|d88probe_test|clidisk_test' --output-on-failure
```

結果:

- RA ON: 8/8 passed
- Normal: 3/3 passed

## 6. 次に実装するもの

次はPhase 4の残りを実装する。

1. macOS `NSURLSession` backendの実HTTPS疎通試験。
2. `RaMediaStore`で得た起動媒体MD5から`RaService::BeginLoadGameByHash()`を呼ぶ
   アプリ統合。
3. エミュレーションフレーム完了後に`RaService::DoFrame()`、一時停止・メニュー中に
   `RaService::Idle()`を呼ぶVM側統合。
4. SDLオーバーレイ通知へ`RaEvent` queueを接続する。
5. request generationとsession generationを`RaService`側の状態更新破棄へさらに接続する。
6. password、token、POST本文をログへ出さないことのテスト。
