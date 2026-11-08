# CYD DOOM — real DOOM on the ESP32-2432S028R

DOOM (shareware Episode 1) running on the Cheap Yellow Display: ESP32-WROOM,
no PSRAM, 320×240 SPI panel, resistive touch. Based on
[GBADoom](https://github.com/doomhack/GBADoom) — doomhack's Game Boy Advance
port of PrBoom — with an ESP32 platform layer written for this board.

This is the **actual engine and port source**, recovered from
`Desktop\cyd-doom-master-BACKUP` (historically `HenrysCat/cyd-doom`). The
handoff ZIP contained only a README; the engine was recovered from the local
backup, not invented.

## Verified build

```text
PlatformIO Core 6.2.0
platform espressif32@6.9.0, board esp32dev, framework arduino
CPU 240 MHz, flash 80 MHz / DIO, -O2
RAM:   113,852 / 327,680 bytes (34.7%)
Flash: 556,953 / 1,048,576 bytes (53.1%)
```

Command: `python -m platformio run` — **PASS**. Hardware execution is
**NOT TESTED** (no serial port was present during recovery).

## Layout

- `src/doom/` — GBADoom engine in C, lightly patched for the ESP32 port.
- `src/port/` — ESP32 platform layer:
  - `main.cpp` — boot, DOOM task on core 1 with a 24 KB static stack
  - `display.cpp` — SPI panel driver; 240×160 8bpp → 320×213 RGB565 scaling
  - `input.cpp` — bit-banged XPT2046, serial keys, BOOT button
  - `setup.cpp` — first-boot orientation/colour and touch calibration wizard
  - `wad_mmap.c` — memory-maps the `wad` flash partition
  - `sram.c` — save games and display configuration in the `sram` partition
  - `i_system_esp32.cpp` — GBADoom platform API, frame push, fps/heap stats
- `include/` — engine headers (patched: `m_fixed.h`, `doom_iwad.h`,
  `gba_functions.h`)
- `partitions.csv` — app 1 MB @0x10000, sram 32 KB @0x110000,
  wad 2.9 MB @0x118000
- `tools/wadtrim.py` — trims a GbaWadUtil-processed WAD to fit flash
- `tools/flashwad.py` — writes the WAD image at 0x118000
- `tools/GbaWadUtil/` — upstream WAD preprocessor (`GbaWadUtil.exe` +
  `Qt5Core.dll`); its bundled `gbadoom.wad` was removed because it is game data

`main.cpp` is the recovered standalone DOOM entry point (formerly
`main.cpp.backup`). The later `main.cpp` in the backup was a CYD OS launcher
stub containing `// Your existing launcher code goes here.`; it is preserved in
`../HISTORICAL_VERSIONS/DOOM_main_launcher_stub.cpp` and is not used.

## Build and flash

```bash
python -m platformio run
python -m platformio run -t upload --upload-port COM6
```

No upload port is pinned in `platformio.ini`; COM6/COM6 are machine-specific.

## Game data (not included)

WAD files are copyrighted game data and are **not** in this repository.
Obtain the DOOM shareware 1.9 IWAD (`doom1.wad`, 4,196,020 bytes) legally,
place it in this directory, then:

```bash
tools\GbaWadUtil\GbaWadUtil.exe -in doom1.wad -out doom1_gba.wad
python tools/wadtrim.py trim doom1_gba.wad doom1_cyd.wad --limit 0x2E8000 --drop-maps E1M6,E1M7,E1M9
python tools/flashwad.py --port COM6
```

`tools/wadtrim.py analyze <wad>` prints a size breakdown per category.

Included maps: E1M1–E1M5 and E1M8. E1M6, E1M7 and E1M9 were dropped for flash
space; E1M5's exit leads to E1M8, and secret exits act as normal exits
(patched in `g_game.c`). Demos, help/credit screens and audio are cut.

## First-boot setup

Hold BOOT while powering on (or boot with an unset configuration):

1. **Orientation/colour** — short-press BOOT to cycle eight combinations.
   Stop when the letter **F** reads upright and the bars are red, green, blue
   from left to right. Hold BOOT for about a second to accept.
2. **Touch calibration** — press and hold each white crosshair until it turns
   green.

The result is stored in flash; later boots go straight into the game.

## Controls

Touch layout during play:

```
+---------------------------+----------------+
| MENU                      |            MAP |   top corners
+------------+--------------+----------------+
|            |                               |
|  virtual   |          USE (open doors)     |
|  d-pad     |                               |
|  forward/  +-------------------------------+
|  back/     |                               |
|  turn      |          FIRE                 |
+------------+-------------------------------+
     left half            right half
```

- Left half: tap toward the edge — up = forward, down = backpedal,
  left/right = turn.
- Menus: FIRE/USE selects, MENU goes back.
- BOOT button opens/closes the menu.

Serial keys at 115200 baud:

| Key | Action |
|---|---|
| `w` / `s` | forward / back |
| `a` / `d` | turn left / right |
| `q` / `e` | strafe |
| `f` | fire |
| space | use / confirm |
| `m` or Enter | menu |
| `t` or Tab | automap |

Eight save slots persist in the `sram` partition across power-off and firmware
re-flashes.

## Changes made during recovery

- Restored the real standalone `main.cpp` (the backup's active file was a
  launcher stub).
- Replaced the 38.4 KB runtime framebuffer `malloc` with a static buffer. The
  original could fail after `Z_Init` fragmented internal RAM — the historical
  ~32,504-byte allocation failure.
- Removed the hardcoded `upload_port`/`monitor_port` values.
- Removed the bundled `gbadoom.wad` (game data) and the WAD files from the
  repository.
- Kept all engine behavior otherwise unchanged; no working code was rewritten.

## Known limitations / not tested

- **Runtime performance is NOT TESTED.** Historical observation: roughly
  26–30 fps, ~72 KB free heap. Do not treat those as current measurements.
- No sound or music: `i_audio.c` is stubbed; the speaker on GPIO26/DAC2 is a
  future project.
- E1M6/E1M7/E1M9 are not playable in the trimmed build.
- The 4 MB board cannot hold the full shareware WAD plus this firmware.
- WAD compatibility was verified against the source pipeline (GbaWadUtil +
  trim), not by running every WAD variant on hardware.

## Credits and license

- DOOM engine: id Software; PrBoom contributors (GPL v2)
- GBA port: [doomhack/GBADoom](https://github.com/doomhack/GBADoom)
- `tools/GbaWadUtil` binary from GBADoom; ships `Qt5Core.dll` (LGPL, The Qt
  Company)
- ESP32/CYD port: this repository
- Engine and port code: GPL v2 — see `LICENSE` and the headers in `src/doom/`.
  Game data is not included.
