#!/usr/bin/env python3
"""Analyze and trim a (GbaWadUtil-processed) Doom WAD to fit an ESP32 flash partition.

Usage:
  python wadtrim.py analyze <in.wad>
  python wadtrim.py trim <in.wad> <out.wad> [--limit BYTES] [--drop-maps E1M6,E1M7,...]

Trimming drops music (D_*), sound effects (DS*/DP*), GENMIDI/DMXGUS,
and optionally whole maps. Lump order is preserved; dropped lumps that the
engine may still look up by name (demos, music) can be kept as zero-length
entries instead of removed so index-based lookups stay valid.
"""
import struct
import sys
from collections import defaultdict

MAP_CHILD = {b'THINGS', b'LINEDEFS', b'SIDEDEFS', b'VERTEXES', b'SEGS',
             b'SSECTORS', b'NODES', b'SECTORS', b'REJECT', b'BLOCKMAP'}


def read_wad(path):
    data = open(path, 'rb').read()
    ident, numlumps, infotableofs = struct.unpack_from('<4sII', data, 0)
    assert ident in (b'IWAD', b'PWAD'), 'not a wad'
    lumps = []
    for i in range(numlumps):
        filepos, size, name = struct.unpack_from('<II8s', data, infotableofs + 16 * i)
        lumps.append([name.rstrip(b'\0'), data[filepos:filepos + size]])
    return ident, lumps


def is_map_marker(name):
    n = name.decode('ascii', 'replace')
    return (len(n) == 4 and n[0] == 'E' and n[2] == 'M' and n[1].isdigit() and n[3].isdigit()) or \
           (len(n) == 5 and n.startswith('MAP') and n[3:].isdigit())


def categorize(lumps):
    cats = defaultdict(int)
    current_map = None
    for name, data in lumps:
        n = name.decode('ascii', 'replace')
        if is_map_marker(name):
            current_map = n
            cats['map:' + n] += len(data)
            continue
        if name in MAP_CHILD and current_map:
            cats['map:' + current_map] += len(data)
            continue
        current_map = None
        if n.startswith('D_'):
            cats['music'] += len(data)
        elif n.startswith('DS'):
            cats['sfx'] += len(data)
        elif n.startswith('DP'):
            cats['pcspeaker'] += len(data)
        elif n.startswith('DEMO'):
            cats['demos'] += len(data)
        elif n in ('GENMIDI', 'DMXGUS'):
            cats['midi-instr'] += len(data)
        else:
            cats['other(gfx/tex/sprites/etc)'] += len(data)
    return cats


def analyze(path):
    ident, lumps = read_wad(path)
    total = sum(len(d) for _, d in lumps) + 12 + 16 * len(lumps)
    print(f'{path}: {ident.decode()} {len(lumps)} lumps, total {total} bytes')
    for k, v in sorted(categorize(lumps).items(), key=lambda kv: -kv[1]):
        print(f'  {k:30s} {v:>9,}')


def black_patch():
    """320x200 solid-black patch: every column offset points at one shared
    200-pixel column of palette index 0. ~1.5KB instead of ~68KB."""
    col_ofs = 8 + 320 * 4
    blob = struct.pack('<hhhh', 320, 200, 0, 0)
    blob += struct.pack('<I', col_ofs) * 320
    blob += b'\x00' + bytes([200]) + b'\x00' + b'\x00' * 200 + b'\x00' + b'\xff'
    return blob


STUB_BLACK = {'HELP1', 'HELP2', 'CREDIT', 'WIMAP0'}


def trim(inpath, outpath, limit, drop_maps):
    ident, lumps = read_wad(inpath)
    out = []
    in_dropped_map = False
    for name, data in lumps:
        n = name.decode('ascii', 'replace')
        if is_map_marker(name):
            in_dropped_map = n in drop_maps
            if in_dropped_map:
                continue
        elif name in MAP_CHILD and in_dropped_map:
            continue
        else:
            in_dropped_map = False

        if n.startswith(('DS', 'DP', 'DEMO')) or n in ('GENMIDI', 'DMXGUS'):
            continue                      # fully removed: never referenced on this port
        if n.startswith('D_'):
            out.append([name, b''])       # keep name so S_ChangeMusic lookups succeed harmlessly
            continue
        if n in STUB_BLACK:
            out.append([name, black_patch()])
            continue
        out.append([name, data])

    # rebuild
    header = 12
    dirsize = 16 * len(out)
    payload = sum((len(d) + 3) & ~3 for _, d in out)
    total = header + dirsize + payload
    print(f'trimmed: {len(out)} lumps, {total:,} bytes (limit {limit:,})')
    if total > limit:
        print('WARNING: still over limit!')

    blob = bytearray()
    blob += struct.pack('<4sII', ident, len(out), header + payload)
    entries = []
    pos = header
    for name, data in out:
        entries.append((pos, len(data), name))
        blob += data
        pad = (-len(data)) % 4
        blob += b'\0' * pad
        pos += len(data) + pad
    for filepos, size, name in entries:
        blob += struct.pack('<II8s', filepos, size, name)
    open(outpath, 'wb').write(blob)
    print(f'wrote {outpath}: {len(blob):,} bytes')
    return len(blob) <= limit


if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'analyze':
        analyze(sys.argv[2])
    elif cmd == 'trim':
        limit = 0x280000
        drop = set()
        args = sys.argv[2:]
        rest = []
        i = 0
        while i < len(args):
            if args[i] == '--limit':
                limit = int(args[i + 1], 0); i += 2
            elif args[i] == '--drop-maps':
                drop = set(args[i + 1].split(',')); i += 2
            else:
                rest.append(args[i]); i += 1
        ok = trim(rest[0], rest[1], limit, drop)
        sys.exit(0 if ok else 1)
