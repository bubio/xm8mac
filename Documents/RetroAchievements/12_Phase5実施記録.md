# Phase 5実施記録: オーバーレイ基盤

## 1. 現在の完了範囲

Phase 5の入口として、Phase 4で`App`へ直書きしていたRA通知状態を
`RaOverlay`へ分離した。

- `Source/RA/ra_overlay.*`を追加した。
- `RaOverlay`は通知本文、通知期限、RAホーム画面で使う状態snapshotを保持する。
- 通知の実描画は従来どおり`App::DrawRaOverlay()`が既存`Font`、`Video`を使って行う。
- `RaOverlay`本体はSDL描画依存を持たないため、RA共通コードの単体テストから利用できる。
- Phase 4で実装した短いRA通知の見た目と表示位置は変更していない。

この段階では、ログイン画面、ASCIIオンスクリーンキーボード、実績一覧、バッジ画像、
Leaderboard画面はまだ実装していない。次工程で`RaOverlay`のsnapshotと入力状態を拡張し、
SDL上のRA専用画面へ進める。

## 2. 実装意図

Phase 5以降では、RAホーム、ログイン、ライブラリ、実績一覧、Leaderboardなどを
エミュレーション画面上のオーバーレイとして実装する必要がある。

そのため、通知文字列や画面状態を`App`の一時変数として増やし続けず、
RA専用の状態オブジェクトへ集約する。描画自体は既存UI資産を使うため、
初期段階では`App`に残す。

`RaOverlay`へUI状態を集約することで、次の作業単位を分けられる。

- RA状態snapshot生成
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
- Phase 4のRA service、media store、memory map関連テストが引き続き通る。

## 4. 検証結果

2026-07-01時点で次を確認した。

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

- RAホーム画面の常設表示。
- password login UI。
- ASCIIオンスクリーンキーボード。
- `SDL_TEXTINPUT`による物理キーボード入力。
- 実績一覧と詳細表示。
- バッジ、アバター画像cacheの描画接続。
- Leaderboard一覧、送信結果、順位表示。
- タッチ操作とゲームコントローラ操作の同等化。
