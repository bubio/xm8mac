# Hardcoreと状態管理

## 1. 目的

Hardcoreは表示上の設定ではなく、エミュレータ操作を制限する実行ポリシーである。
すべての操作入口を共通`RaSessionPolicy`へ通し、メニュー、ショートカット、CLI、
Android lifecycleの間で挙動を一致させる。

## 2. 操作マトリクス

| 操作 | Normal | Softcore | Hardcore | Offline session |
|---|---:|---:|---:|---:|
| 手動ステート保存 | 許可 | 許可 | 禁止 | 許可、offline専用 |
| 手動ステート読込 | 許可 | 許可 | 禁止 | 許可、offline専用 |
| 終了時自動ステート | 許可 | 初期版は禁止 | 禁止 | 初期版は禁止 |
| 起動時自動ステート | 許可 | 初期版は禁止 | 禁止 | 初期版は禁止 |
| Full Speed | 許可 | 許可 | 禁止 | 許可 |
| 通常速度以外 | 現行仕様 | 許可 | 禁止 | 現行仕様 |
| Pseudo fast disk access | 許可 | 禁止 | 禁止 | 現行仕様 |
| デバッガ・breakpoint・frame step・メモリ書換え | 許可 | 禁止 | 禁止 | 許可 |
| Reset | 許可 | 許可、RAへ通知 | 許可、RAへ通知 | 許可 |
| 一時停止 | 許可 | 許可 | `rc_client_can_pause()`次第 | 許可 |
| 同一D88内bank切替 | 許可 | 許可 | 許可 | 許可 |
| 同一ゲーム別D88交換 | 許可 | RA確認後許可 | RA確認後許可 | ローカル所属確認後許可 |
| 別ゲームD88挿入 | 許可 | セッション再開始 | セッション再開始 | セッション再開始 |
| システム/クロック変更 | 現行仕様 | セッション再開始 | セッション再開始 | 現行仕様 |
| 設定メニュー | 許可 | 許可 | 非停止表示、制限あり | 許可 |

## 3. モード開始と終了

### 3.1 Softcore開始

1. RAログインを確認する。
2. D88作業コピーを確定する。
3. Pseudo fast diskを一時的に無効化する。
4. `rc_client_set_hardcore_enabled(client, 0)`を明示する。
5. RAゲーム識別とセッション開始を完了する。
6. 作業コピーを挿入する。
7. VMをcold resetし、`rc_client_reset()`を1回呼んでフレーム評価を開始する。

既存の自動ステートを無条件でロードしない。ライブラリから新規起動する場合は
常にcold resetとし、ユーザーが明示的にLoad Stateを選択した場合だけロードする。

### 3.2 Hardcore開始

Hardcore開始時はゲームを未ロード状態にして次を行う。

1. Full Speedを解除し、通常速度へ戻す。
2. 保留中のステートロードを破棄する。
3. Pseudo fast diskを一時的に無効化する。
4. `rc_client_set_hardcore_enabled(client, 1)`を呼ぶ。
5. RAゲーム識別とセッション開始を完了する。
6. 作業コピーを挿入する。
7. VMをcold resetし、`rc_client_reset()`を1回呼んで最初のフレームから評価を始める。

`rc_client`はHardcore既定ONだが、前回状態へ依存せず毎回手順4を実行する。ゲーム未ロード時の
`rc_client_set_hardcore_enabled()`は`RC_CLIENT_EVENT_RESET`を発生させないため、通常起動で
RESETイベントを待ってはならない。ロード済みゲーム中にRESETイベントを受けた場合の
防御処理は[7. Reset](#7-reset)に従う。

### 3.3 終了

1. フレーム実行を止める。
2. 進行中HTTPをセッション世代番号で無効化する。
3. `rc_client_unload_game()`を呼ぶ。
4. VMから媒体を閉じ、D88バッファの書き戻しを完了させる。
5. ライブラリの最終起動日時を更新する。
6. 初期版ではRAモードの自動ステートを保存しない。
7. RA開始前のPseudo fast disk設定値を復元する。

Hardcore終了時には自動ステートを保存しない。

## 4. ステートファイル拡張

### 4.1 互換性方針

- 既存XM8ステートの先頭構造を破壊しない。
- 既存VM/設定/媒体データの後ろへRA chunkと固定長footerを追加する。
- RA chunkがない旧ステートはNormalでは従来どおり読める。
- RAモードではRA chunkがない旧ステートを拒否し、Normalで読み込むよう案内する。
- Hardcoreでは新旧を問わずステート読込を開始前に拒否する。
- RA手動stateはNormal stateと共有せず、Softcoreは
  `ra/states/<ra_game_id>/<anchor_md5>/state<slot>.bin`、Offline sessionは
  `ra/states/offline/<anchor_md5>/state<slot>.bin`へ保存する。

### 4.2 RA chunk形式

```text
4 bytes  magic "XMRA"
4 bytes  little-endian chunk version (=1)
4 bytes  chunk size（magicからCRC32まで、footerを含まない）
8 bytes  state body size（ファイル先頭からRA chunk直前まで）
4 bytes  state body CRC32
4 bytes  RA game ID（Offline stateは0）
1 byte   saved mode (1=Softcore, 3=Offline)
3 bytes  reserved, zero
32 bytes anchor media MD5 ASCII, NULなし
4 bytes  rc_version() integer
4 bytes  serialized progress size
N bytes  rc_client serialized progress
4 bytes  CRC32 from magic through progress
4 bytes  footer magic "XMRF"
4 bytes  chunk size（headerと同じ値）
```

制約:

- すべての整数をlittle-endianで保存する。
- `serialized progress size`上限は16MiBとする。
- Offline stateのprogress sizeは0とする。
- save末尾8バイトのfooterからchunk先頭を特定し、chunk size、chunk CRC、state body size、
  state body CRC、Game ID、媒体MD5を
  VM、設定、DiskManagerへ1バイトも適用する前に検証する。
- state body sizeはRA chunk先頭offsetと完全一致しなければならない。
- headerとfooterのchunk size不一致、範囲外size、CRC不一致、未知mode、予約byte非0は
  state全体を拒否する。
- `rc_client_progress_size()`で必要量を取得し、
  `rc_client_serialize_progress_sized()`を使う。
- 復元には`rc_client_deserialize_progress_sized()`を使う。
- 非推奨のsizeなしAPIは使用しない。

### 4.3 Softcoreロード手順

1. 現在のRA Game IDとステートのGame IDが一致することを確認する。
2. anchor媒体MD5が現在のゲームに所属することを確認する。
3. state body sizeとCRC32を確認する。不一致ならstateをロードしない。
4. `rc_version()`が保存値と一致することを確認する。不一致ならstateをロードしない。
5. VM、設定、媒体ステートをロードする。
6. RA progressを復元する。
7. metadata検証後の`rc_client_deserialize_progress_sized()`だけが失敗した場合は、
   VMロードを維持して`rc_client_reset()`し、RA進捗をresetした旨を通知する。

別ゲームのステートはRAモード中にロードしない。ゲームを終了してNormalで開くか、
対象ゲームをライブラリから開始するよう案内する。

旧stateや破損RA chunkをSoftcoreでロードしない理由は、現行stateがDiskManagerとFDC内部に
D88 pathと書込みbufferを含み、RAメタデータなしでは現在の作業コピーへ安全に束縛できない
ためである。`rc_client_reset()`だけでは原本書戻しと別ゲームメモリの評価を防げない。

### 4.4 Offline stateロード手順

- 現在の状態が`OfflineSession`、saved modeが3、Game IDが0、anchor MD5が現在媒体と
  一致する場合だけロードする。
- state body size、CRC、chunk、footerはSoftcoreと同じように事前検証する。
- Offline stateをActiveなSoftcore/Hardcoreセッションへロードしない。
- Offline stateにはRA progressがないため、`rc_client_deserialize_progress_sized()`を呼ばない。

## 5. 既存自動ステート

- RAモードの手動・自動ステートを既存`state*.bin`と共有しない。
- 初期実装ではRAモードの自動ロードをOFFとし、手動ステートだけを提供する。
- Normalの既存自動ステート処理には変更を加えない。
- `App::Run()`開始時の`Load(0)`と終了時の`Save(0)`は、実効モードがNormalの場合だけ
  従来どおり実行する。Softcore、Hardcore、Offline sessionでは両方を呼ばない。
- 将来RA自動ステートを有効化する場合もHardcoreでは作成・読込しない。

## 6. 一時停止

### 6.1 ユーザー操作

Hardcore中にVM停止を要求する操作では、停止前に
`rc_client_can_pause(client, &frames_remaining)`を呼ぶ。

- true: VMを停止し、停止中は`rc_client_idle()`を呼ぶ。
- false: VMを停止せず、残り時間を表示する。
- RAフルオーバーレイ自体は非停止で表示できるため、この判定を必要としない。
- ファイル選択など停止が必要なOS画面は、許可が得られるまで開かない。

### 6.2 アプリ非アクティブ

最小化、電話着信、Android background等は拒否できないため次の順序とする。

1. VMフレーム実行を止める。
2. 音声を停止する。
3. lifecycleが生きている間はmain threadで最大1秒間隔で`rc_client_idle()`を呼ぶ。
   RA active時は現行の無期限`SDL_WaitEvent()`をtimeout付き待機へ変更する。
4. 復帰時に経過時間だけでVMを早送りしない。
5. RAセッションがサーバー側で失効していた場合、RAを無効化してゲームを継続する。

バックグラウンド移行をHardcore違反とはみなさないが、ステート保存で復帰を実現しない。
Androidプロセスが終了した場合、そのHardcoreセッションは終了する。

## 7. Reset

- ユーザーReset、システム変更に伴うReset、Hardcore有効化Resetを区別する。
- VMをresetした直後に`rc_client_reset()`を1回呼ぶ。
- 同一操作で複数のReset経路が発火してもRA resetは1回にまとめる。
- `RC_CLIENT_EVENT_RESET`の処理中に再度`rc_client_set_hardcore_enabled()`を呼ばない。
- ロード済みゲームで`RC_CLIENT_EVENT_RESET`を受けた場合は次のVMフレームを実行する前に
  VMをcold resetし、直後に`rc_client_reset()`を呼ぶ。通常のHardcore起動はこのeventに
  依存しない。
- Softcore/Hardcoreどちらでもreset後に実績評価を継続する。

## 8. Full Speedと速度

- Hardcore開始前に現行の`App::NormalSpeed()`を呼び、Full Speed状態を必ず解除する。
- Hardcore中はメニュー、ショートカット、CLI、内部APIからのFull Speed要求を拒否する。
- フレームスキップ表示設定は、エミュレーション時間を速めない限り許可する。
- 音声同期の揺らぎや描画落ちは速度変更とみなさない。
- デバッグ用ターボ、テスト用環境変数はRAビルドのリリース実行中に有効化できないようにする。

## 8.1 エミュレーション補助機能

- 現行のPseudo fast diskは`config.ignore_crc`によりCRCエラーを無視し、FDC event間隔も
  短縮する。オンラインRAセッション開始前に一時的にOFFへ固定し、Softcore/Hardcore中の
  設定変更を拒否する。
- RAセッション終了またはOffline session移行時に、RA開始前の設定値を復元する。
- デバッガ、breakpoint、frame step、メモリ書換えAPIはSoftcore/Hardcoreのどちらでも
  使用させない。現行XM8に公開UIがない場合も、リリースビルドのCLIや隠しshortcutから
  到達できないことを確認する。
- 将来これらの診断機能を公開するときは`RaSessionPolicy`を経由し、RAを終了して
  Offline sessionへ移行しない限り入口を有効にしない。

## 9. 媒体変更

### 9.1 bank切替

同一`working.d88`内のbank切替はRAハッシュを変えない。既存の遅延bank切替処理を
使用し、`rc_client_begin_change_media()`は呼ばない。

### 9.2 同一ゲームの別媒体

1. 変更先媒体が現在の`games.id`に所属することをDBで確認する。
2. 媒体のRA Game ID一致と作業コピーのD88 probeを確認する。
3. MD5を`rc_client_begin_change_media()`へ渡す。
4. 成功時だけVMの対象Driveを交換する。
5. VM交換失敗時は旧MD5へRA mediaをrollbackする。
6. rollback失敗時はRA評価を停止してOffline sessionへ移り、現在のDrive内容を維持する。

### 9.3 別ゲーム

現在のRAセッションを維持したまま挿入しない。ゲーム終了確認後、
新しいゲームとして識別、セッション開始、VM resetを行う。

## 10. 設定とシステム変更

次の変更はRAゲームの再起動を必要とする。

- PC-88 boot mode
- CPU clock
- 拡張RAM構成
- RA mode
- Unofficial achievements
- Encore mode

音量、画面サイズ、scanline、入力割当、通知時間、画像キャッシュ上限は
ゲーム実行中に変更できる。

## 11. 失敗時の扱い

- RA state metadata、footer、CRC、version不一致: VMを変更せずloadを拒否する。
- metadata検証後のRA progress復元失敗: VMロードを維持し、RA runtimeをresetする。
- Hardcore制約判定の内部エラー: 安全側として操作を拒否する。
- `rc_client_do_frame()`に必要なメモリが読めない: RAログへ記録し、該当アドレスを無効として返す。
- セッション開始後の通信切断: `ActiveDisconnected`で継続する。
- サーバーがセッションを再開できない: `rc_client_unload_game()`してframe評価を止め、
  Offline session表示へ切り替える。
- Hardcore中にRAを無効化した場合、そのセッションでSoftcore解除へ切り替えない。
  Hardcore表示と制限を解除し、Offline sessionのNormal相当policyへ移る。

## 12. テスト観点

- 全操作入口でHardcore禁止が一致する。
- Hardcore開始時にFull Speedと保留ステートが残らない。
- Softcore/Hardcore開始時にPseudo fast diskが無効となり、終了時に元の設定へ戻る。
- オンラインRAセッション中にデバッガまたはメモリ書換えへ到達できない。
- RESETイベント1回に対しVM resetとRA resetが各1回である。
- 旧ステート、正常RA chunk、破損chunk、別ゲームchunkをVM変更前に正しく分岐する。
- RA手動stateがNormalの`state*.bin`を上書きしない。
- 一時停止拒否時にVMが実際には停止していない。
- Android background中に自動ステートが作成されない。
- 同一D88のbank切替でmedia changeが発生しない。
- 別D88交換失敗時に現在のDrive内容を失わない。
