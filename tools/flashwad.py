#!/usr/bin/env python3
"""Flash the trimmed WAD image into the ESP32's 'wad' partition.

Usage: python tools/flashwad.py [--port COM6] [--wad doom1_cyd.wad]
Offsets must match partitions.csv.
"""
import argparse
import os
import subprocess
import sys

WAD_OFFSET = '0x118000'

parser = argparse.ArgumentParser()
parser.add_argument('--port', default=None, help='serial port, e.g. COM6')
parser.add_argument('--wad', default=os.path.join(os.path.dirname(__file__), '..', 'doom1_cyd.wad'))
args = parser.parse_args()

if not args.port:
    parser.error('--port is required, for example: --port COM6')

cmd = [sys.executable, '-m', 'esptool', '--chip', 'esp32', '--port', args.port,
       '--baud', '921600', 'write_flash', WAD_OFFSET, args.wad]
print(' '.join(cmd))
sys.exit(subprocess.call(cmd))
