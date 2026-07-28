# Generated D88 fixtures

The fixture generator creates deterministic D88 files from code. It does not
read BIOS ROMs, commercial disk images, or any other external content.

Build and generate the standard set:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target generate_d88_fixtures
./build/generate_d88_fixtures /tmp/xm8-ra-fixtures
```

The output set is:

| File | Purpose | Size | MD5 |
|---|---|---:|---|
| `single.d88` | One medium, one bank, one sector | 960 | `5c50ca4f9e3a7afbe4d6666e8974949d` |
| `second.d88` | Distinct one-bank medium | 960 | `ff400f51a2567419b3778691a905952e` |
| `multi.d88` | The two banks concatenated in one D88 | 1920 | `9be57f249da12241c8785db0b195216b` |
| `pair.m3u` | `single.d88` and `second.d88` | 50 | Not an RA medium |

`d88fixture_test` verifies structure, bank count, M3U resolution, and byte-for-byte
determinism. `d88write_test` modifies the generated sector through XM8's existing
`DISK` implementation, closes and reopens the image, and verifies persistence.

Generated files are temporary test output and must not be committed. If the
generator changes intentionally, update this table and the Phase 0 record in the
same commit.
