# Phase 4実施記録: macOS HTTP・認証

## 1. 現在の完了範囲

Phase 4のうち、実RAサーバーへ接続しない共通土台を追加した。

- `RaHttpClient`契約を追加した。
- fake HTTP backendを追加した。
- `credentials.bin`の保存、読込、削除を追加した。
- RA ON buildだけへ上記を組み込み、Normal buildには組み込んでいない。

`NSURLSession`実装、`rc_client_server_call_t` adapter、password/token loginの
`rc_client`接続は未実装である。

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

## 4. テスト

`Tests/ra_credentials_http_test.cpp`を追加した。

検証内容:

- credentials保存、読込、削除。
- Unix系で0600作成されること。
- credentialsの不正lengthを拒否すること。
- oversized username/tokenを拒否すること。
- token clear APIでメモリ上のtokenが空になること。
- HTTP fake backendがGET、空POST、通常POSTを区別すること。
- request bodyが所有コピーになっていること。
- cancel後の遅延responseが二重通知されないこと。

実行済み:

```text
cmake -S . -B build-ra -DXM8_ENABLE_RETROACHIEVEMENTS=ON
cmake --build build-ra --target ra_credentials_http_test ra_library_store_test xm8
ctest --test-dir build-ra -R 'ra_credentials_http_test|ra_library_store_test|ra_media_probe_test|ra_memory_map_test|ra_dependency_test|d88fixture_test|d88probe_test' --output-on-failure
cmake --build build --target xm8
ctest --test-dir build -R 'd88fixture_test|d88probe_test|clidisk_test' --output-on-failure
```

結果:

- RA ON: 7/7 passed
- Normal: 3/3 passed

## 5. 次に実装するもの

次はPhase 4の残りを実装する。

1. `NSURLSession`を使うmacOS HTTP adapter。
2. API redirect禁止、画像HTTPS redirect制限、oversize、timeout、TLS失敗の分類。
3. `rc_client_server_call_t` adapter。
4. request generation、cancel、shutdown順序。
5. password login、token login、logoutの`rc_client`接続。
6. token拒否時の`credentials.bin`削除。
7. password、token、POST本文をログへ出さないことのテスト。
