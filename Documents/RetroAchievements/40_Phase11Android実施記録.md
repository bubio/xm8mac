# Phase 11 Android移植 実施記録

## 状態

2026-07-25にAndroid用RetroAchievementsのビルド経路、HTTPS/JNI通信、Keystore資格情報、
SAFからworking copyへの読込経路を実装した。現時点の判定は **実装継続中** とする。
実機・emulatorでのRAログイン、媒体取込み、ライフサイクル受入は未実施であり、Phase 11の
完了判定はまだ行わない。

## 実装内容

- Gradleの`XM8_ENABLE_RETROACHIEVEMENTS`でRA ON/OFFを切り替える。通常版はAPI 19、
  RA有効版はKeystoreとGCMを使用できるAPI 23を最低SDK／`APP_PLATFORM`にする。
- activeな`ndk-build`経路へRA source、rcheevos、SQLite、stbをRA ON時だけ追加した。
  非RA buildにはRA sourceと依存ライブラリを含めない。非activeなCMake経路にも同じ定義を
  用意した。
- Javaの`HttpURLConnection`をexecutorで実行し、64 bit transport request IDでJNIへ完了を返す。
  HTTPS以外を拒否し、既定のTLS／hostname検証を使い、redirectを追跡しない。timeout、cancel、
  response上限、HTTP status、content typeをnative queueへ返す。
- service通信と画像通信の複数clientをtransport IDで区別し、完了callbackが誤ったclientへ
  配送されないようにした。Activity破棄時は接続とexecutorを停止する。
- tokenはAndroid KeystoreのAES-GCM鍵で暗号化し、app-private SharedPreferencesへ保存する。
  backup／device transferからRA directory、username hint、暗号化credentialを除外した。
- `content://` URIをnative filesystem pathとして渡さず、既存SAFのfile descriptor経由で
  読み込む。RA媒体のhash／probe／copyはこの読込を使い、VMへはapp-private `working.d88`を
  渡す。権限が失効しても、すでに作成済みのworking copyはそのまま起動できる。
- Androidのconnectivity monitorは外部probeを発行せず、OSの接続状態を500 msでcacheする。

## 実行済み検証

| 構成 | 結果 |
|---|---|
| Android Debug、RA OFF、4 ABI | 成功 |
| Android lintDebug、RA OFF | 成功 |
| Android Debug、RA ON、4 ABI | 成功 |
| macOS RA ON build／CTest | 成功、25/25 |

RA ON APKには`armeabi-v7a`、`arm64-v8a`、`x86`、`x86_64`の各`libmain.so`が含まれることを確認した。

## 未実施受入

- API 22／36での起動、ログイン、ゲーム開始
- SAF単体D88、M3U、tree import、権限取消後の再指定
- background／resume、回転、low-memory、process recreation
- touchだけのログイン操作とOS software keyboard
- RA ON/OFFのRelease build、実HTTPS、instrumented smoke

これらを実機またはemulatorで確認し、既対応OSの必要な共通回帰を完了した時点でPhase 11を完了とする。
