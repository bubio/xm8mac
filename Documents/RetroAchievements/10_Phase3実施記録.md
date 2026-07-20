# Phase 3実施記録: macOSライブラリとD88作業コピー基盤

## 位置づけ

この記録は `05_実装工程とテスト計画.md` の Phase 3「SQLiteライブラリと作業コピー」の完了記録である。

既存の通常モードではD88を従来どおり扱う。RA有効ビルドで、かつRA設定が有効な場合だけ、D88原本をRA媒体ストアへ登録し、VMへ渡すパスを `ra/media/<md5>/working.d88` に置き換える。

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

> 2026-07-19追記: これらの列と検証は既存DB互換のため維持するが、初期版UIには公開しない。
> 実行時はUnofficial／Encore／SpectatorをOFF、通知を5秒、画像cacheを128MiBに固定する。

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
- `RaLibrary::MergeGameMedia()` を追加し、別ローカルゲームとして登録された媒体を明示的に既存ゲームへ統合できるようにした。
- 統合時は移動元ゲームの起動構成を削除し、媒体ordinalを移動先ゲーム末尾へ付け替え、移動元ゲームを削除する。

RA起動用パス解決の基盤を追加した。

- `RaMediaStore::ResolveLaunchProfile()` はDrive 1/2の起動構成を `working.d88` の実パスへ解決する。
- 返却する各Driveのパスは必ずRA root配下の `media/` 相対パスから構成される。
- 作業コピーが消失またはprobe不能で、原本が登録時MD5と一致する場合は起動前に再作成する。
- 原本が消失または変更されていても、作業コピーがprobe可能なら既存セーブからの起動を妨げない。

### RA rootとApp接続

`Source/RA/ra_paths.*` を追加し、既存 `Setting::GetSettingDir()` の直下にRA rootを置く規則を固定した。

```text
<setting_dir>/ra/
```

`setting.bin`の形式は変更しない。RA有効/無効と前回RA modeは `ra/library.sqlite3` の `ra_settings` だけに保存する。

RA有効ビルドでは `App` が起動時にRAライブラリを開き、`ra_settings.enabled` がtrueの場合だけD88オープン経路をRAモードにする。

- RA mode OFF: 従来どおりユーザー指定D88を `DiskManager` へ渡す。
- RA mode ON: ユーザー指定D88を `RaMediaStore::ImportDesktopD88()` で登録し、作業コピーを `DiskManager` へ渡す。
- RA DB初期化失敗時はRA modeを無効化し、既存起動処理を継続する。

### ビルド接続

`ThirdParty/CMakeLists.txt` の `xm8_ra_core` に `ra_library.cpp`、`ra_media_store.cpp`、`ra_paths.cpp` を追加した。

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
- `Setting::GetSettingDir()` 配下の `ra/` をRA rootとして解決する。
- 起動構成解決でDrive 1/2とも `ra/media/` 配下の作業コピーだけが返る。
- 作業コピー消失時、原本が登録時MD5と一致すれば起動構成解決で再作成される。
- 別ゲームとして登録された媒体を既存ゲームへ明示統合できる。
- 明示統合後も移動先ゲームのRA anchorは維持される。

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

## Phase 3完了範囲

Phase 3として、次を完了した。

- SQLite schema、RA設定、破損DB隔離。
- 単体D88、M3U、フォルダ再帰取り込み。
- 作業コピー生成、重複再利用、セーブデータ初期化。
- 媒体状態検査。
- 起動構成保存、RA anchor不変条件、既存ゲーム同士の媒体統合。
- RA root解決。
- RA mode ON時のD88直接起動から作業コピー経由のVM挿入。

次はPhase 4以降で扱う。

- RA API通信、認証、ゲーム識別、RA Game IDによる自動統合。
- ライブラリ画面、媒体状態表示、作業コピー再作成の確認UI。
- オーバーレイからのD88/M3U/フォルダ取り込み操作。
- Android SAF URI記録とストリームコピー。

## 後続実装への注意

RA active mediaは起動時に識別した媒体へ固定する。補助ドライブの挿入だけでRA active mediaを変更してはいけない。

作業コピーはセーブデータを含む運用ファイルである。既存作業コピーに対して、現在の原本MD5との一致を要求して自動上書きする実装に変更してはいけない。

ファイル名推測だけで複数ディスクを自動統合してはいけない。統合の優先順位は計画書どおり、M3U、RA Game ID一致、ユーザー編集の順とする。

## 後続仕様変更

このPhase 3実施時点では新規設定の`last_mode`をSoftcoreとしていたが、2026-07-20に
新規環境の初期選択をHardcoreへ変更した。RA全体の初期値OFFと、既存DBに保存済みの
モードを維持する仕様は変更していない。詳細は
[35_Hardcore初期値とメインメニューRA表示.md](35_Hardcore初期値とメインメニューRA表示.md)を参照する。
