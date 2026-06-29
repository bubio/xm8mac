# Phase 3実施記録: macOSライブラリとD88作業コピー基盤

## 位置づけ

この記録は `05_実装工程とテスト計画.md` の Phase 3「SQLiteライブラリと作業コピー」のうち、VM起動経路へ接続する前の基盤実装を対象とする。

本段階ではRAモードの実行時挙動はまだ変更しない。既存の通常モードではD88を従来どおり扱い、RA有効ビルドでのみ新規のRAライブラリ/媒体ストアコードをビルド・テストできる状態にした。

## 実装内容

### RaLibrary

`Source/RA/ra_library.*` を追加し、RA用保存領域の初期化とSQLite DBの基本スキーマ作成を実装した。

保存領域は呼び出し側が渡すRA root配下に固定し、次のディレクトリを作成する。

```text
ra/
  library.sqlite3
  media/
  temp/
  images/
  states/
```

DBには計画書で定義した主要テーブルの初期形を作成する。

- `schema_meta`
- `ra_settings`
- `games`
- `media`
- `media_banks`
- `launch_profiles`
- `progress`
- `image_cache`
- `sync_state`

`ra_settings` は初期状態でRA無効、前回モードはSoftcore相当として1行だけ作成する。

RA設定のロード/保存APIを追加した。設定値は`setting.bin`には保存せず、`ra/library.sqlite3`
内の`ra_settings`だけで保持する。

現在保存する値は次のとおり。

- RA有効/無効
- 前回モード: SoftcoreまたはHardcore
- Unofficial、Encore、Spectator
- 通知表示秒数: 3、5、8秒
- 画像キャッシュ上限: 64、128、256MiB

不正なmode、通知秒数、画像キャッシュ上限はDBへ書き込む前に拒否する。

DB open時に`PRAGMA integrity_check`を実行する。`SQLITE_CORRUPT`または`SQLITE_NOTADB`相当の
破損を検出した場合は、`library.sqlite3`、`library.sqlite3-wal`、`library.sqlite3-shm`を
`.corrupt.<unix_time>`付きのファイル名へ隔離してから新規DBを作成する。

未知のschema versionは破損扱いにしない。仕様どおりRAライブラリopen失敗として扱い、
自動隔離や自動downgradeは行わない。

### RaMediaStore

`Source/RA/ra_media_store.*` を追加し、デスクトップ上のD88原本をRA管理媒体として取り込む処理を実装した。

取り込み処理は次の順序で固定する。

1. 原本D88を `ProbeD88File()` で検査し、D88全体MD5、サイズ、bank数、bank名を取得する。
2. `ra/temp/` に一時コピーを作成する。
3. 一時コピーを再度 `ProbeD88File()` で検査する。
4. MD5、サイズ、bank数が原本検査結果と一致する場合だけ `ra/media/<md5>/working.d88` へ配置する。
5. SQLiteへ `games`、`media`、`media_banks`、`launch_profiles` を登録する。
6. 同一MD5が登録済みならDB行と既存作業コピーを再利用する。

作業コピーが既に存在する場合は、保存データを含む可能性があるため原本MD5との一致を要求しない。D88として検査可能であればそのまま再利用する。検査不能な場合だけ原本から再作成する。

原本ファイルが更新されてMD5が変わった場合は別媒体として登録する。既存作業コピーは暗黙移行しない。

### ビルド接続

`ThirdParty/CMakeLists.txt` の `xm8_ra_core` に `ra_library.cpp` と `ra_media_store.cpp` を追加した。

`CMakeLists.txt` に `ra_library_store_test` を追加した。

macOS 10.13ターゲットを維持するため、本実装とテストでは `std::filesystem` を使用しない。パス結合、ディレクトリ作成、ファイル存在確認、mtime取得、削除は当面POSIX APIで行う。

## テスト

`Tests/ra_library_store_test.cpp` を追加した。

このテストでは以下を検証する。

- RA rootを開くと `library.sqlite3` が作成される。
- RA設定の既定値がRA無効、Softcoreである。
- RA設定を保存し、再open後も値が保持される。
- 不正なRA modeは保存前に拒否される。
- SQLiteではない`library.sqlite3`が隔離され、新規DBで復旧する。
- D88単体を取り込むと既知のD88全体MD5でDB登録される。
- 初回取り込みでは `ra/media/<md5>/working.d88` が作成される。
- 作業コピー作成直後の内容は原本と一致する。
- 原本D88は取り込み後も変更されない。
- 同一D88の再取り込みでは既存DB行と既存作業コピーを再利用する。
- 原本を変更してMD5が変わった場合は別媒体として登録する。
- 既存作業コピーは新しい媒体取り込みで上書きされない。

## 検証結果

RA有効構成:

```text
cmake --build build-ra --target ra_library_store_test ra_media_probe_test ra_dependency_test xm8
ctest --test-dir build-ra -R 'ra_dependency_test|ra_media_probe_test|ra_memory_map_test|ra_library_store_test|d88fixture_test|d88probe_test' --output-on-failure
```

結果: 6件すべて成功。

通常構成:

```text
cmake --build build --target xm8
ctest --test-dir build -R 'd88fixture_test|d88probe_test' --output-on-failure
```

結果: 2件すべて成功。

## 未実装

次の項目はPhase 3の残作業、または後続Phaseで実装する。

- アプリ設定ディレクトリから実際のRA rootを決定する `ra_settings`/設定層接続。
- D88/M3U/フォルダ再帰走査のUIまたは起動経路からの登録。
- M3Uの先頭D88をRA識別基準媒体にする処理。
- 複数媒体を同一ゲームへ統合する編集処理。
- 原本消失、原本更新、作業コピー破損をライブラリ画面へ表示する処理。
- RAモードで未登録D88を直接開いた場合の自動登録から起動までの接続。
- DiskManager/VMへ作業コピーだけを渡す接続。
- Android SAF URI記録とストリームコピー。

## 後続実装への注意

RA active mediaは起動時に識別した媒体へ固定する。補助ドライブの挿入だけでRA active mediaを変更してはいけない。

作業コピーはセーブデータを含む運用ファイルである。既存作業コピーに対して、現在の原本MD5との一致を要求して自動上書きする実装に変更してはいけない。

ファイル名推測だけで複数ディスクを自動統合してはいけない。統合の優先順位は計画書どおり、M3U、RA Game ID一致、ユーザー編集の順とする。
