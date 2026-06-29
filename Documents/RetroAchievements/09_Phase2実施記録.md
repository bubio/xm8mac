# Phase 2実施記録: macOSメモリとD88ハッシュ

## 1. 結果

Phase 2は2026-06-30に実施した。対象はmacOS基準実装だけであり、Windows、Linux、
AndroidのRA build定義は変更していない。

| 項目 | 結果 |
|---|---|
| RAメモリ公開 | `PC88`/`VM`へ副作用なしのinspection read APIを追加 |
| 公開範囲 | `0x00000-0x0ffff` Main RAM、`0x10000-0x10fff` Text VRAM |
| D88識別 | `rcheevos` `RC_CONSOLE_PC8800`のwhole-file MD5を使用 |
| D88解析 | bank数、bank名、file size、MD5を`RaMediaProbe`で取得 |
| RA ON test追加 | `ra_media_probe_test`、`ra_memory_map_test` |
| RA OFF影響 | RA依存物なしで既存Normal build成功 |

`RetroAchivementsIntegrationHandbook`の内容を確認し、次の設計規則をPhase 2の実装判断へ反映した。

- emulator coreへ入れる変更は副作用なしメモリ検査APIに限定する。
- D88 hashや媒体解析は`Source/RA/`側へ置き、VMや既存UIへ持ち込まない。
- rcheevosのhash実装を正とし、XM8独自MD5と結果が分岐しないようにする。

## 2. 追加・変更した実装

### `RaMediaProbe`

`Source/RA/ra_media_probe.*`を追加した。責務は次のとおり。

- `HashPc8800File()`: `rc_hash_generate_from_file()`へ`RC_CONSOLE_PC8800`を指定する。
- `ProbeD88File()`: D88 headerを順に検査し、bank数、bank名、file size、whole-file MD5を返す。
- invalid D88、open失敗、hash失敗は`false`とerror文字列で返す。

この段階では作業コピー生成、SQLite登録、M3U group化はまだ実装しない。それらはPhase 3で扱う。

### RAメモリ読み出し

`PC88`に次を追加した。

```cpp
size_t read_ra_inspection_memory(uint32 addr, uint8 *buffer, size_t count) const;
```

`VM`にも同名の委譲APIを追加した。読み取りは`ReadPc88RaInspectionMemory()`へ集約し、次の範囲だけを公開する。

| RAアドレス | XM8実体 |
|---|---|
| `0x00000-0x0ffff` | `PC88::ram[0x10000]` |
| `0x10000-0x10fff` | `PC88::tvram[0x1000]` |

CPU論理アドレス、bank切替後のread bank、I/O、wait、DMA、GVRAMは読まない。範囲外から開始した場合は
0 byte、範囲内から範囲外へまたぐ場合は読めたbyte数だけを返す。

## 3. テスト

RA ON buildで次を確認した。

```sh
cmake -S . -B build-ra \
  -DXM8_ENABLE_RETROACHIEVEMENTS=ON \
  -DBUILD_TESTING=ON \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$PWD/build/_deps/sdl2-src"
cmake --build build-ra --target ra_memory_map_test ra_media_probe_test ra_dependency_test xm8
ctest --test-dir build-ra \
  -R 'ra_dependency_test|ra_media_probe_test|ra_memory_map_test|d88fixture_test|d88probe_test' \
  --output-on-failure
```

結果:

- `ra_dependency_test`: 成功。
- `ra_media_probe_test`: 成功。
- `ra_memory_map_test`: 成功。
- `d88probe_test`: 成功。
- `d88fixture_test`: 成功。

`ra_media_probe_test`ではfixtureの既知MD5を検証した。

| fixture | 期待MD5 | bank数 |
|---|---|---:|
| `single.d88` | `5c50ca4f9e3a7afbe4d6666e8974949d` | 1 |
| `second.d88` | `ff400f51a2567419b3778691a905952e` | 1 |
| `multi.d88` | `9be57f249da12241c8785db0b195216b` | 2 |

`ra_memory_map_test`ではRAM先頭、RAM/TVRAM境界、TVRAM末尾、範囲外、null buffer、
0 byte readを検証した。

RA OFF buildでは次を確認した。

```sh
cmake --build build --target xm8
ctest --test-dir build -R 'd88fixture_test|d88probe_test' --output-on-failure
```

結果:

- `xm8`: build成功。
- `d88probe_test`: 成功。
- `d88fixture_test`: 成功。

## 4. 残タスク

Phase 3で、`RaMediaProbe`を使ってSQLiteライブラリ、M3U登録、作業コピー生成、
原本MD5再検証へ進む。Phase 2では実際のゲーム起動経路、DiskManagerへの作業コピー接続、
RA session開始はまだ行わない。
