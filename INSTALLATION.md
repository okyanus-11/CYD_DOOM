# Installation and flashing

## Requirements
- ESP32-2432S028R Cheap Yellow Display and USB data cable
- Python 3.10+ and PlatformIO Core (`python -m pip install --upgrade platformio`)
- A legally obtained DOOM shareware 1.9 IWAD; game data is not included

## Clone and upload firmware

```bash
git clone https://github.com/okyanus-11/CYD_DOOM.git
cd CYD_DOOM
pio run
pio run -t upload --upload-port COMx
```

Replace `COMx` with the board's actual serial port (for example, `COM6` on Windows). On Linux, use the correct device path.

## Prepare and flash game data

Place your legally obtained shareware IWAD in the repository root as `doom1.wad`, then:

```powershell
tools\GbaWadUtil\GbaWadUtil.exe -in doom1.wad -out doom1_gba.wad
python tools/wadtrim.py trim doom1_gba.wad doom1_cyd.wad --limit 0x2E8000 --drop-maps E1M6,E1M7,E1M9
python tools/flashwad.py --port COMx
```

The WAD is written to the dedicated flash partition. Never distribute copyrighted WAD data. See README for first-boot calibration and controls.
