# XM8 Core Migration Scope (UI-Preserved)

This document defines how to migrate from `common_source_project` into XM8 without changing files under `Source/UI`.

## Rules

1. Keep `Source/UI` unchanged.
2. Prefer same-path/same-name files from `common_source_project/src`.
3. Do not import features that are not used by current XM8 UX/settings unless needed for build or correctness.
4. Keep existing XM8 runtime behavior first, then add optional features deliberately.

## Required Compatibility Surface

The core must continue to provide the interfaces used by XM8 UI:

- `VM::frame_rate()`
- `VM::open_disk()`, `VM::close_disk()`
- `VM::play_tape()`, `VM::rec_tape()`, `VM::close_tape()`
- `VM::save_state()`, `VM::load_state()`
- `VM::get_device(id)` with current ID assumptions used by UI
- `EMU::set_key_buffer()`, `EMU::set_joy_buffer()`
- `config` fields used by UI (`dipswitch`, `sound_device_type`, `scan_line`, `ignore_crc`, etc.)

## Out of Scope by Default

The following should not be migrated unless explicitly required:

- Non-PC-8801 machine features
- Win32-only OSD/frontend pieces (`win32/osd.*`, `winmain.cpp`)
- UI feature sets not exposed in XM8 UI (extra media/device options)
- Optional peripherals not required by current XM8 workflow

## Migration Order

1. Compatibility helpers (no behavior change)
2. Small common components (`fifo`, selected device helpers)
3. Medium components (`event`, `disk`, `upd765a`, `z80`)
4. PC-8801 core (`pc88`, `pc8801`) with XM8 behavior preserved

## Acceptance per Step

- `cmake --build build -j8` succeeds
- XM8 launches
- Disk mount/eject works
- Tape play/rec works
- Save/load state works
- No UI source changes

## Progress Notes

- 2026-02-12:
  - Added `DEVICE` compatibility aliases used by common-source style code:
    `process_state()`, `initialize_output_signals()`, event/clock alias methods,
    interrupt alias methods, and `set_device_name()/get_device_name()`.
  - Added `DISK` compatibility wrappers:
    `open(const _TCHAR*)`, `process_state()`, and utility aliases
    (`set_data_crc_error()`, `get_usec_per_track()`, `get_bytes_per_usec()`).
  - Added `UPD765A` compatibility wrappers:
    `open_disk(const _TCHAR*)`, `process_state()`,
    `is_disk_inserted()`, `is_disk_protected()`, `get_media_type()`,
    and safer `get_disk_handler()` bounds check.
  - Verified with `cmake --build build -j8` (success, warnings only).
