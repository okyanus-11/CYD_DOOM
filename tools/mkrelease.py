#!/usr/bin/env python3
"""Package a prebuilt-firmware release into dist/.

Produces:
  dist/cyd-doom-firmware.bin   merged image, flash at offset 0x0
  dist/<individual .bin files> for anyone who prefers explicit offsets

Run after `python -m platformio run`. The WAD is deliberately NOT part of a
release (modified shareware data may not be redistributed) - users create
doom1_cyd.wad with tools/wadtrim.py and flash it with tools/flashwad.py.
"""
import glob
import os
import shutil
import subprocess
import sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
build = os.path.join(root, '.pio', 'build', 'cyd')
dist = os.path.join(root, 'dist')

# boot_app0.bin lives in the PlatformIO arduino framework package
boot_app0 = glob.glob(os.path.expanduser(
    '~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'))

parts = [
    ('0x1000', os.path.join(build, 'bootloader.bin')),
    ('0x8000', os.path.join(build, 'partitions.bin')),
]
if boot_app0:
    parts.append(('0xe000', boot_app0[0]))
parts.append(('0x10000', os.path.join(build, 'firmware.bin')))

for _, p in parts:
    if not os.path.exists(p):
        sys.exit(f'missing {p} - run "python -m platformio run" first')

os.makedirs(dist, exist_ok=True)
for _, p in parts:
    shutil.copy(p, dist)

merged = os.path.join(dist, 'cyd-doom-firmware.bin')
cmd = [sys.executable, '-m', 'esptool', '--chip', 'esp32', 'merge_bin',
       '-o', merged, '--flash_mode', 'dio', '--flash_size', '4MB']
for off, p in parts:
    cmd += [off, p]
subprocess.check_call(cmd)

print(f'\nrelease files in {dist}:')
for f in sorted(os.listdir(dist)):
    print(f'  {f}  ({os.path.getsize(os.path.join(dist, f)):,} bytes)')
print('\nflash with:')
print('  python -m esptool --chip esp32 --port COMx --baud 921600 write_flash 0x0 cyd-doom-firmware.bin')
