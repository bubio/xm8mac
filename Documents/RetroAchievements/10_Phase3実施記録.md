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

M3U取り込みの基盤を追加した。既存`LoadM3U()`を使い、M3Uに記載された順にD88を登録する。

- 先頭D88をRA識別基準媒体、ローカルゲーム、Drive 1の起動構成として扱う。
- 2件目以降のD88は先頭D88と同じ`game_id`へ追加し、`ordinal`へM3U上の順序を保存する。
- M3U行の`#0`などのbank接尾辞はD88ファイルパスから分離し、媒体ハッシュには含めない。
- 先頭D88の登録に失敗した場合、後続D88へ自動フォールバックしない。
- 既に別ゲームへ登録済みの媒体をM3Uで別ゲームへ暗黙移動しない。

フォルダ再帰登録の基盤を追加した。

- `.m3u` / `.M3U` と `.d88` / `.D88` を再帰走査する。
- ディレクトリシンボリックリンクは辿らず、通常ファイルだけを候補にする。
- 候補数は最大10000件とし、超過時は登録を中断してエラーにする。
- 登録順はM3Uを先、D88単体を後に固定する。
- ファイル名推測による自動統合は行わない。M3U、既存登録済みMD5、後続のRA Game ID照合、ユーザー編集だけを統合根拠にする。

セーブデータ初期化の基盤を追加した。

- 原本D88を再probeし、登録時MD5と一致する場合だけ新しい作業コピーを作る。
- 既存`working.d88`は一時退避し、新しいコピー配置に失敗した場合は退避ファイルを戻す。
- 原本が更新されMD5が変わった場合は初期化を拒否する。既存セーブを変更後原本へ暗黙移行しない。
- 実行中VMの停止確認やユーザー確認UIは呼び出し側の責務とする。

媒体状態検査の基盤を追加した。

- `RaMediaStore::CheckMediaHealth()` は登録済み媒体の原本と作業コピーを検査する。
- 原本のsize/mtimeが登録値と同じ場合は原本MD5を再計算しない。
- 原本のsize/mtimeが変化した場合だけ原本を再probeし、同じMD5ならmetadataを更新する。
- 原本MD5が変化した場合は `health_state=2` とし、既存作業コピーは上書きしない。
- 作業コピーは現在MD5ではなくD88としてprobe可能かだけで判定する。
- 作業コピーが消失またはprobe不能な場合は、UIが再作成または初期化操作を提示するための状態としてDBへ保存する。

起動構成APIの基盤を追加した。

- `RaLibrary::LoadLaunchProfile()` と `RaLibrary::SaveLaunchProfile()` を追加した。
- Drive番号は内部値どおり0/1で保持する。
- `SaveLaunchProfile()` はDrive 1/2の構成を1トランザクションで置換する。
- RA anchorは必ず1件だけ必要とし、0件または2件の構成は保存前に拒否する。
- 別ゲーム媒体や存在しないbankはSQLiteの外部キー制約で拒否する。

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
- M3U取り込みでは先頭D88をanchorとし、2枚目を同じローカルゲームへ登録する。
- M3U内のbank接尾辞つきD88パスを媒体ファイルとして解決できる。
- フォルダ再帰登録はM3Uを先に処理し、D88単体を後で処理する。
- フォルダ再帰登録は大文字拡張子 `.D88` / `.M3U` を受け付ける。
- 作業コピーに書込みがあっても原本は変更されない。
- セーブデータ初期化は登録時MD5と一致する原本からだけ `working.d88` を再作成する。
- 原本が更新されMD5が変わった場合、セーブデータ初期化を拒否する。
- 原本を変更してMD5が変わった場合は別媒体として登録する。
- 既存作業コピーは新しい媒体取り込みで上書きされない。
- 媒体状態検査は正常、作業コピー消失、原本変更を区別する。
- 起動構成は標準でDrive 1にRA anchorを持ち、Drive 2の明示割当を保存できる。
- RA anchorが0件の起動構成は拒否される。

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
ctest --test-dir build -R 'd88fixture_test|d88probe_test|clidisk_test' --output-on-failure
```

結果: 3件すべて成功。

## 未実装

次の項目はPhase 3の残作業、または後続Phaseで実装する。

- アプリ設定ディレクトリから実際のRA rootを決定する `ra_settings`/設定層接続。
- D88/M3U/フォルダ再帰走査のUIまたは起動経路からの登録。
- 複数媒体を同一ゲームへ統合する編集処理。
- 原本消失、原本更新、作業コピー破損をライブラリ画面へ表示する処理。
- RAモードで未登録D88を直接開いた場合の自動登録から起動までの接続。
- DiskManager/VMへ作業コピーだけを渡す接続。
- Android SAF URI記録とストリームコピー。

## 後続実装への注意

RA active mediaは起動時に識別した媒体へ固定する。補助ドライブの挿入だけでRA active mediaを変更してはいけない。

作業コピーはセーブデータを含む運用ファイルである。既存作業コピーに対して、現在の原本MD5との一致を要求して自動上書きする実装に変更してはいけない。

ファイル名推測だけで複数ディスクを自動統合してはいけない。統合の優先順位は計画書どおり、M3U、RA Game ID一致、ユーザー編集の順とする。
