#!/usr/bin/env python3
"""Instrument guarded CSZO4 Ver. B weapon and native hit-dispatch sites.

The generated upstream inputs remain unchanged. Fail closed on any source drift.
No ROM, binary image or game assets are read or written by this tool.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path

# SHA-256 of the upstream generated source, not game content.
SOURCE_HASHES = {
    '00': 'fdb10f64f1aebf37cf8f6d555da099be06c8bc701a20bc1aac56601d1c325703',
    '08': 'afa45e48b185643d3c5d4ce2f223e3ed1c1b11c3bcab109fc2d0747e32c8bf4e',
}
PATCHES = {
    '00': [
        ('  A_END_80006B70: ;', '  A_END_80006B70: ; RS8(0x10, (uint64_t)(int64_t)csz_weapons_hit_axis((uint32_t)RG4(0xFC),0,(int32_t)RG4(0x14)));'),
        ('  A_END_80006B74: ;', '  A_END_80006B74: ; RS8(0x18, (uint64_t)(int64_t)csz_weapons_hit_axis((uint32_t)RG4(0xFC),1,(int32_t)RG4(0x1C)));'),
        ('A_80004BD0: RR_INS(0x80004BD0U);',
         'A_80004BD0: { uint32_t w_ = csz_weapons_filter((uint32_t)RG4(0x24)); if(w_ != (uint32_t)RG4(0x24)) RS8(0x20,w_); } RR_INS(0x80004BD0U);'),
    ],
    '08': [
        ('A_800950A4: RR_INS(0x800950A4U);', 'A_800950A4: ; csz_weapons_hit_begin(); RR_INS(0x800950A4U);'),
        ('A_800950BC: RR_INS(0x800950BCU);', 'A_800950BC: { uint32_t w_ = csz_weapons_hit_flags((uint32_t)RG4(0x14)); if(w_ != (uint32_t)RG4(0x14)) RS8(0x10,(uint64_t)(int64_t)(int32_t)w_); } RR_INS(0x800950BCU);'),
        ('A_800950D8: RR_INS(0x800950D8U);', 'A_800950D8: ; csz_weapons_hit_complete(); RR_INS(0x800950D8U);'),
        ('A_800950F0: RR_INS(0x800950F0U);', 'A_800950F0: ; csz_weapons_hit_end(); RR_INS(0x800950F0U);'),
        ('  A_END_80090968: ;', '  A_END_80090968: ; csz_weapons_init((uint32_t)RG4(0x24));'),
        ('  A_END_800913F8: ;', '  A_END_800913F8: ; csz_weapons_reset((uint32_t)RG4(0x1C));'),
        ('  A_END_800914A0: ;', '  A_END_800914A0: ; csz_weapons_reload((uint32_t)RG4(0x14));'),
        ('A_800947F0: RR_INS(0x800947F0U);',
         'A_800947F0: ; csz_weapons_begin((uint32_t)RG4(0xE4)); RR_INS(0x800947F0U);'),
        ('  A_END_80094A38: ;', '  A_END_80094A38: ; csz_weapons_accepted((uint32_t)RG4(0x1C));'),
    ],
}

def instrument(data: bytes, part: str) -> bytes:
    if part not in SOURCE_HASHES:
        raise ValueError('Unsupported lifted part: ' + part)
    if hashlib.sha256(data).hexdigest() != SOURCE_HASHES[part]:
        raise ValueError('Lifted source revision mismatch: ' + part)
    text = data.decode('utf-8')
    anchor = '#include "crszone_lifted.h"'
    if text.count(anchor) != 1:
        raise ValueError('Unexpected include layout')
    text = text.replace(anchor, anchor + '\n#include "crisis_zone_weapons.h"', 1)
    for old, new in PATCHES[part]:
        if text.count(old) != 1:
            raise ValueError('Hook site is missing or ambiguous: ' + old)
        text = text.replace(old, new, 1)
    return text.encode('utf-8')

def prepare(source: Path, output: Path) -> None:
    # Check every input before writing anything.
    files = [(f'crszone_lifted_{part}.c', instrument(
        (source / f'crszone_lifted_{part}.c').read_bytes(), part)) for part in SOURCE_HASHES]
    output.mkdir(parents=True, exist_ok=True)
    for name, data in files:
        target = output / name
        if not target.exists() or target.read_bytes() != data:
            tmp = target.with_suffix('.tmp')
            tmp.write_bytes(data)
            tmp.replace(target)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.source, args.output)
