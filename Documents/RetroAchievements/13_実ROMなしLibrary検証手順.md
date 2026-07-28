# 実ROMなしLibrary検証手順

## 1. 目的

RA対応PC-8801ゲームを入手できない場合でも、RetroAchievements Library v1の
表示、選択、起動、作業コピー保護を確認できるようにする。

この手順では著作物を含まない生成D88 fixtureを使い、通常のRA DBへ
RA識別済みゲーム相当の行をseedする。アプリ本体にはfake RA UIやテスト専用画面を
追加しない。

## 2. Seed tool

RA有効buildで次の開発用ターゲットを使う。

```sh
cmake --build build-ra --target ra_seed_library_fixture xm8
```

seed先はXM8設定ディレクトリ、またはRA rootを直接指定する。

```sh
./build-ra/ra_seed_library_fixture --setting-dir "<XM8 setting directory>"
./build-ra/ra_seed_library_fixture --ra-root "<XM8 setting directory>/ra"
```

toolは次を行う。

- `ra/dev-fixtures/source/`へ著作物なしD88 fixtureを生成する。
- `pair.m3u`を`RA Test Multi Disk`として登録する。
- `multi.d88`を`RA Test Game`として登録する。
- それぞれにRA Game ID、badge URL、progressを注入する。
- RA working copy path、原本fixture path、媒体MD5を標準出力へ表示する。

## 3. 手動確認

1. seed toolを実行する。
2. `xm8`を起動する。
3. `RetroAchievements > Library`を開く。
4. `RA Test Game`と`RA Test Multi Disk`がRAタイトルで表示されることを確認する。
5. Libraryで各ゲームを選択してGame Detailを開き、STARTから起動できることを確認する。
   その際、VMへ`ra/media/<md5>/working.d88`が渡ることを確認する。
6. `ra/dev-fixtures/source/`配下の原本fixtureが変更されていないことを確認する。

実RAサーバーとの疎通、実績一覧、badge画像取得はこの手順の目的外である。
それらは`FakeRaHttpClient`を使う自動テストと、実対応ゲームを入手できた場合の
任意smoke testで確認する。

## 4. 自動テスト

seed helperは次のテストで検証する。

```sh
ctest --test-dir build-ra -R 'ra_seed_library_fixture_test|ra_library_store_test|ra_overlay_test|ra_media_probe_test|ra_service_test|d88probe_test' --output-on-failure
```

確認内容:

- seed後の`ListGames()`がRA識別済みfixtureだけを返す。
- single D88、M3U/複数媒体、progressあり、health OKの表示用値が揃う。
- `ResolveLaunchProfile()`がworking copy pathを返し、原本fixture pathを返さない。
