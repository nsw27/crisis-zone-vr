#!/usr/bin/env python3
"""Read-only Quest log/scene-trace collection and conservative offline analysis.

Python 3.9+, standard library only. No device writes, ROM reads or uploads.
Uses the existing authorised Android Debug Bridge installation.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import math
import re
import shutil
import statistics
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

PACKAGES = ('org.crisiszone.vr.preview2', 'org.crisiszone.vr.preview', 'org.crisiszone.vr')
FILES = ('crisiszone-vr.log', 'crisiszone-vr-previous.log', 'quest-scene-trace.csv')
MAX_BYTES = 16 * 1024 * 1024
EXPECTED_FIELDS = 'frame,entry,model,polygons,x,y,z,bounds_valid,left,top,right,bottom'.split(',')
NUMBER = r'(?:\d+(?:\.\d+)?)'
PERF = re.compile(
    r'\[QPERF\] SCENE CPU (' + NUMBER + r') MS/EYE \| GPU (n/a|' + NUMBER +
    r') MS/EYE; RENDER (' + NUMBER + r') FPS \| GAME (' + NUMBER +
    r') FPS; SCENE (\d+) X (\d+) \((\d+)%\) \| TRI (\d+); UPLOAD (' + NUMBER +
    r') KB/S \| WORLD DRAWS (' + NUMBER + r')/FRAME')
FRAME = re.compile(r'^# frame=(\d+),submitted_entries=(\d+),built_polygons=(\d+),truncated_model_records=(\d+)$')
DECLARED = re.compile(r'^# frames=(\d+),records=(\d+)$')


def read_text(path: Path) -> str:
    if path.stat().st_size > MAX_BYTES:
        raise ValueError(f'{path.name}: exceeds the 16 MiB analysis limit')
    return path.read_text(encoding='utf-8-sig', errors='replace')


def distribution(values: list[float]) -> dict[str, Any]:
    if not values:
        return {'samples': 0, 'min': None, 'median': None, 'max': None}
    return {'samples': len(values), 'min': min(values),
            'median': round(statistics.median(values), 4), 'max': max(values)}


def analyse_log(text: str) -> dict[str, Any]:
    """Never reinterpret averaged/filtered diagnostic samples as raw frame times."""
    blocks: list[dict[str, Any]] = []
    unmatched = 0
    for line in text.splitlines():
        if '[QPERF]' not in line:
            continue
        match = PERF.search(line)
        if not match:
            unmatched += 1
            continue
        cpu, gpu, render, game, w, h, density, tri, kb, draws = match.groups()
        key = (int(w), int(h), int(density))
        sample = {'scene_cpu_ms_per_eye': float(cpu),
                  'scene_gpu_ms_per_eye': None if gpu == 'n/a' else float(gpu),
                  'render_rate': float(render), 'published_scene_rate': float(game),
                  'triangles': int(tri), 'upload_kib_per_s': float(kb),
                  'scene_draws_per_rendered_frame': float(draws)}
        if not blocks or tuple(blocks[-1]['scene']) != key:
            blocks.append({'scene': list(key), 'samples': []})
        blocks[-1]['samples'].append(sample)
    for block in blocks:
        samples = block.pop('samples')
        block['sample_count'] = len(samples)
        block['stats'] = {k: distribution([s[k] for s in samples if s[k] is not None]) for k in samples[0]}
        block['gpu_unavailable_samples'] = sum(s['scene_gpu_ms_per_eye'] is None for s in samples)
    return {'sample_count': sum(b['sample_count'] for b in blocks), 'blocks': blocks,
            'unrecognised_metric_lines': unmatched,
            'interpretation': 'Consecutive scene-size/density blocks, not matched gameplay benchmarks. CPU/GPU values are filtered scene-only samples per eye, not whole-frame times. No 1% lows, frame-time percentiles, compositor FPS or causal bottleneck diagnosis can be calculated from these samples.'}


def analyse_trace(text: str) -> dict[str, Any]:
    if len(text.encode('utf-8')) > MAX_BYTES:
        raise ValueError('Scene trace exceeds the 16 MiB analysis limit')
    frames: dict[int, dict[str, int]] = {}
    declared = None
    data: list[str] = []
    for line in text.splitlines():
        if line.startswith('#'):
            m = FRAME.fullmatch(line)
            if m:
                f, submitted, built, truncated = map(int, m.groups())
                if f in frames or f >= 600:
                    raise ValueError('Duplicate or out-of-range frame metadata')
                frames[f] = {'submitted_entries': submitted, 'built_polygons': built, 'truncated': truncated}
            m = DECLARED.fullmatch(line)
            if m:
                if declared is not None:
                    raise ValueError('Duplicate capture header')
                declared = tuple(map(int, m.groups()))
        elif line.strip():
            data.append(line)
    reader = csv.DictReader(io.StringIO('\n'.join(data)))
    if reader.fieldnames != EXPECTED_FIELDS:
        raise ValueError('Unexpected scene-trace columns; this tool expects the Preview 2 format')
    records: dict[int, list[dict[str, Any]]] = {}
    seen: set[tuple[int, int]] = set()
    totals = {'records': 0, 'zero_built_records': 0, 'duplicate_model_instances': 0,
              'valid_bounds_records': 0, 'bounds_outside_arcade': 0}
    for raw in reader:
        if None in raw or any(v is None for v in raw.values()):
            raise ValueError('Incomplete or extra scene-trace columns')
        row: dict[str, Any] = {k: int(raw[k]) for k in EXPECTED_FIELDS[:8]}
        bounds = [float(raw[k]) for k in EXPECTED_FIELDS[8:]]
        f, entry = row['frame'], row['entry']
        if not (0 <= f < 600 and 0 <= entry <= 65535 and 0 <= row['model'] <= 65535
                and 0 <= row['polygons'] <= 65535 and row['bounds_valid'] in (0, 1)):
            raise ValueError('Out-of-range scene-trace record')
        if (f, entry) in seen:
            raise ValueError('Duplicate frame-local render entry; refusing to double count')
        seen.add((f, entry))
        if not all(math.isfinite(v) for v in bounds):
            raise ValueError('Non-finite screen bounds in scene trace')
        if row['bounds_valid'] and (bounds[0] > bounds[2] or bounds[1] > bounds[3]):
            raise ValueError('Inverted screen bounds in scene trace')
        row['bounds'] = bounds if row['bounds_valid'] else None
        records.setdefault(f, []).append(row)
        totals['records'] += 1
        totals['zero_built_records'] += row['polygons'] == 0
        if row['bounds_valid']:
            totals['valid_bounds_records'] += 1
            # A projected box outside the old arcade viewport can still be visible in VR.
            totals['bounds_outside_arcade'] += bounds[2] < 0 or bounds[0] >= 640 or bounds[3] < 0 or bounds[1] >= 480
        if totals['records'] > 65536:
            raise ValueError('Trace exceeds the Preview 2 record cap')
    warnings = []
    nf = declared[0] if declared else len(frames)
    if not 0 <= nf <= 600:
        raise ValueError('Out-of-range declared frame count')
    header_ok = declared is not None and declared[1] == totals['records'] and set(frames) == set(range(nf))
    if not header_ok:
        warnings.append('Incomplete or inconsistent capture header; presence comparisons are disabled.')
    if set(records) - set(frames):
        warnings.append('Some records have no frame metadata; presence comparisons are disabled.')
        header_ok = False
    invalid_frames = set()
    for f, info in frames.items():
        rows = records.get(f, [])
        if any(r['entry'] >= info['submitted_entries'] for r in rows) or sum(r['polygons'] for r in rows) > info['built_polygons'] or len(rows) + info['truncated'] > info['submitted_entries']:
            invalid_frames.add(f)
            warnings.append(f'Frame {f} counters are inconsistent; comparisons involving it are disabled.')
        totals['duplicate_model_instances'] += len(rows) - len({r['model'] for r in rows})
    complete = {f for f, meta in frames.items() if not meta['truncated'] and f not in invalid_frames}
    transitions = []
    # Only compare adjacent complete frames, by MODEL ID SET, never by entity identity.
    if header_ok:
        for f in range(1, nf):
            if f not in complete or f - 1 not in complete:
                continue
            before = {r['model'] for r in records.get(f - 1, [])}
            after = {r['model'] for r in records.get(f, [])}
            added, removed = sorted(after - before), sorted(before - after)
            if added or removed:
                transitions.append({'frame': f, 'newly_present_model_ids': added,
                                    'no_longer_present_model_ids': removed})
    detail = []
    for f in sorted(records):
        rows = records[f]
        zero = [r for r in rows if not r['polygons']]
        if zero:
            detail.append({'frame': f, 'records_without_built_polygons': len(zero),
                           'model_ids': sorted({r['model'] for r in zero})})
    return {'declared_frames': nf, 'header_consistent': header_ok, **totals,
            'truncated_frames': [f for f, m in frames.items() if m['truncated']],
            'presence_change_count': len(transitions), 'presence_changes': transitions,
            'frames_with_zero_built_records': detail, 'warnings': warnings,
            'interpretation': 'Model IDs are shared assets, not persistent enemy IDs. Presence changes are render-list observations, not proof of spawning. Zero built polygons do not by themselves prove incorrect culling. Bounds outside 640x480 do not establish invisibility in the headset. Trace frames are relative to capture start; this format cannot establish exact wall-clock correspondence with performance samples.'}


def analyse_folder(folder: Path) -> dict[str, Any]:
    result: dict[str, Any] = {'logs': {}, 'trace': None, 'warnings': []}
    for name in FILES[:2]:
        path = folder / name
        if path.is_file():
            result['logs'][name] = analyse_log(read_text(path))
    trace = folder / FILES[2]
    if trace.is_file():
        try:
            result['trace'] = analyse_trace(read_text(trace))
        except (ValueError, KeyError, csv.Error) as exc:
            result['warnings'].append(f'Trace not analysed: {exc}')
    return result


def report_markdown(report: dict[str, Any]) -> str:
    lines = ['# Crisis Zone Quest diagnostics', '',
             'This is a local diagnostic report, not a headset benchmark or proof of a pop-in fix.', '']
    for name, log in report['logs'].items():
        lines += [f'## {name}', '', f"Recognised performance samples: **{log['sample_count']}**.", '', log['interpretation'], '']
        if not log['sample_count']:
            lines += ['Enable Graphics > Performance panel, resume gameplay, then pause after the test. There are no usable performance samples in this log.', '']
        for i, block in enumerate(log['blocks'], 1):
            w, h, d = block['scene']
            lines += [f'### Consecutive block {i}: {w} x {h}, {d}% density', '',
                      f"Samples: {block['sample_count']}; unavailable GPU samples: {block['gpu_unavailable_samples']}.", '',
                      '| Observed sample metric | Minimum | Median | Maximum |', '|---|---:|---:|---:|']
            for metric, stats in block['stats'].items():
                def fmt(x: Any) -> str:
                    return 'n/a' if x is None else f'{x:g}'
                lines.append(f"| {metric} | {fmt(stats['min'])} | {fmt(stats['median'])} | {fmt(stats['max'])} |")
            lines.append('')
    if report['trace']:
        t = report['trace']
        lines += ['## Scene trace', '', t['interpretation'], '',
                  f"Frames: {t['declared_frames']}; model-entry records: {t['records']}; zero-built records: {t['zero_built_records']}; truncated frames: {len(t['truncated_frames'])}.", '',
                  f"Adjacent complete-frame model-presence changes: {t['presence_change_count']}. Full observations are in summary.json.", '']
        lines += [f"Warning: {w}" for w in t['warnings']]
    else:
        lines += ['## Scene trace', '', 'No usable scene trace. In Handling, enable Scene trace; resume and reproduce the issue; then pause to flush the capture before collecting.', '']
    lines += [f"Warning: {w}" for w in report['warnings']]
    return '\n'.join(lines) + '\n'


def adb_run(adb: str, args: list[str]) -> bytes:
    completed = subprocess.run([adb, *args], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30, check=False)
    if completed.returncode:
        raise RuntimeError(completed.stderr.decode('utf-8', 'replace').strip() or completed.stdout.decode('utf-8', 'replace').strip() or 'ADB command failed')
    return completed.stdout


def choose_device(adb: str, serial: str | None) -> str:
    output = adb_run(adb, ['devices']).decode('utf-8', 'replace')
    devices = {}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[1] in ('device', 'unauthorized', 'offline', 'recovery', 'sideload'):
            devices[parts[0]] = parts[1]
    if serial:
        if devices.get(serial) != 'device':
            raise RuntimeError('The selected device is unavailable or unauthorised. Check the headset USB debugging prompt.')
        return serial
    if len(devices) != 1:
        raise RuntimeError('Connect one authorised headset, or select it explicitly with --serial from adb devices.')
    device, state = next(iter(devices.items()))
    if state != 'device':
        raise RuntimeError('Headset is not authorised/online. Accept USB debugging in the headset; no authorisation is bypassed.')
    return device


def collect(adb: str, package: str, serial: str | None, output: Path) -> dict[str, Any]:
    if package not in PACKAGES:
        raise ValueError('Only the three named Crisis Zone packages are supported')
    device = choose_device(adb, serial)
    prefix = ['-s', device, 'shell', '-T']
    adb_run(adb, [*prefix, 'run-as', package, 'pwd'])
    meta: dict[str, Any] = {'package': package, 'collected_utc': datetime.now(timezone.utc).isoformat(),
                           'files': {}, 'unavailable': {}, 'writes_to_headset': False, 'automatic_upload': False}
    for name in FILES:
        try:
            content = adb_run(adb, [*prefix, 'run-as', package, 'head', '-c', str(MAX_BYTES + 1), 'files/' + name])
            if len(content) > MAX_BYTES:
                raise ValueError('File exceeds 16 MiB; not saved as a misleading partial capture')
            if not content:
                raise ValueError('File is empty')
            (output / name).write_bytes(content)
            meta['files'][name] = {'bytes': len(content), 'sha256': hashlib.sha256(content).hexdigest()}
        except (RuntimeError, ValueError) as exc:
            meta['unavailable'][name] = str(exc)
    if not meta['files']:
        raise RuntimeError('No logs/capture could be read. Launch Preview 2, reproduce the issue, pause, and retry. ' + json.dumps(meta['unavailable']))
    (output / 'collection.json').write_text(json.dumps(meta, indent=2) + '\n', encoding='utf-8')
    return meta


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--from-folder', type=Path, help='Analyse existing files instead of contacting a device')
    parser.add_argument('--adb', help='Path to the existing adb executable (otherwise search PATH)')
    parser.add_argument('--serial', help='Explicit adb device selection; not stored in the report')
    parser.add_argument('--package', choices=PACKAGES, default=PACKAGES[0])
    parser.add_argument('--out', type=Path, help='New/empty output directory')
    args = parser.parse_args()
    try:
        out = args.out or Path('quest-diagnostics-' + datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S'))
        if out.exists() and (not out.is_dir() or any(out.iterdir())):
            raise ValueError('Output must be a new or empty directory; existing captures are never overwritten')
        if args.from_folder:
            if not args.from_folder.is_dir():
                raise ValueError('--from-folder must name an existing directory')
            report = analyse_folder(args.from_folder)
            if not report['logs'] and report['trace'] is None:
                raise ValueError('No usable Crisis Zone diagnostic files in this folder. ' + '; '.join(report['warnings']))
            out.mkdir(parents=True, exist_ok=True)
        else:
            adb = args.adb or shutil.which('adb')
            if not adb:
                raise ValueError('ADB not found. Use --adb with the adb.exe already used for SideQuest/sideloading, or put Android Platform Tools on PATH.')
            out.mkdir(parents=True, exist_ok=True)
            collect(adb, args.package, args.serial, out)
            report = analyse_folder(out)
        (out / 'summary.json').write_text(json.dumps(report, indent=2, allow_nan=False) + '\n', encoding='utf-8')
        (out / 'REPORT.md').write_text(report_markdown(report), encoding='utf-8')
        print('Saved local report:', (out / 'REPORT.md').resolve())
        print('Nothing was uploaded. Review game logs before sharing the folder.')
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as exc:
        print('Diagnostic collection failed:', exc, file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
