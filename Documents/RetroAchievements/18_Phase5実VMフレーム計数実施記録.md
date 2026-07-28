# Phase 5 実VMフレーム計数実施記録

## 1. 目的

`HostFrameCallback`単体testだけでは、実際の`EVENT::drive()`、音声buffer補充、描画skipを
経由した通知回数を保証できない。このため、実PC-8801MA VMを起動して完了VMフレーム数と
host callback回数の一致を検証するintegration testを追加した。

## 2. test構成

`event_host_frame_integration_test`は、製品と同じPC-8801MAのdevice、CPU、EVENT、sound
実装をリンクする。window、video、実ROMは必要とせず、`EMU`のhost境界だけをtest内で
最小実装する。

音声は48 kHz、2400 samplesで初期化する。PC-8801MAの62.422 Hzでは1 VM frameあたり
約769 samplesとなるため、`create_sound32()`の1回の呼出しで複数回の`drive()`が必要になる。

## 3. 固定した境界

- `VM::run()`による1 frame完了でcallbackがちょうど1回呼ばれる。
- `create_sound32()`が内部で完了した複数frameの数とcallback回数が一致する。
- `request_skip_frames()`／`now_skip()`で描画skipになってもcallbackは抑止されない。
- 待機を挟まず`create_sound32()`と`create_sound32_after()`を連続実行するFull Speed相当経路でも、
  完了frame総数とcallback総数が一致する。

Full SpeedはApp側の描画・音声待機方針であり、別のVM実行関数ではない。そのためtestでは
UI flag自体ではなく、Full Speed時と同じ「音声待機を挟まない連続EVENT処理」を固定した。
Appからcallback経由で`RaService::DoFrame()`が各時点のmemoryを読むことは、既存の
`ra_service_test`と合わせて検証する。

## 4. 検証結果

2026-07-17にmacOSで確認した。

```sh
cmake --build build-ra --target xm8 event_host_frame_integration_test
ctest --test-dir build-ra --output-on-failure
cmake --build build
ctest --test-dir build --output-on-failure
git diff --check
```

- RA ON app build: 成功。
- RA ON test: 19件中19件成功。
- RA OFF app build: 成功。
- RA OFF test: 9件中9件成功。
- 実VM integration testはRA ON/OFFの双方で成功。
- production codeの追加変更はなく、testとCMake登録だけを追加。

## 5. Phase 5残件と次回作業

実VMフレーム計数は完了した。媒体競合中の起動禁止は
[19_Phase5媒体競合起動制限実施記録.md](19_Phase5媒体競合起動制限実施記録.md)で実装した。
競合種類の判定と、統合・分離を明示的に選べるUIとtransactionは
[20_Phase5媒体競合解消実施記録.md](20_Phase5媒体競合解消実施記録.md)で実装した。

その後の順序:

1. ASCENDで実HTTPS Library同期、Game ID、title、progress反映を手動確認する。
2. Phase 5の完了判定を行う。
3. Phase 6のtoast queue、画像cache、全入力、視覚受入へ進む。

実ネットワーク切断時の`ActiveDisconnected`と再接続は
[21_Phase5接続監視修正実施記録.md](21_Phase5接続監視修正実施記録.md)で確認済みである。

媒体競合解決では、自動統合しない既存方針を維持する。競合状態のままRAゲームを起動したり、
別Game IDの媒体を同一ゲームへ暗黙にまとめたりしない。
