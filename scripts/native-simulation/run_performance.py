"""One visible, isolated 60-second Player120 pacing run using current graphics settings.

Performance only: ordinary audio worker, no semantic snapshots or hash qualification.
Keeps constant-size timing counters and a 64-interval failure tail in the engine.
"""
import argparse
import ctypes
import hashlib
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]


def untracked_source_hashes(directory):
    names = subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard', '-z'],
                                    cwd=directory).decode().split('\0')
    return {name: hashlib.sha256((directory / name).read_bytes()).hexdigest()
            for name in names if name}


class ProcessMemory(ctypes.Structure):
    _fields_ = [('cb', ctypes.c_ulong), ('faults', ctypes.c_ulong)] + [
        (name, ctypes.c_size_t) for name in ('peak_working', 'working', 'peak_paged', 'paged',
                                           'peak_nonpaged', 'nonpaged', 'pagefile', 'peak_pagefile', 'private')]


def wait_with_process_metrics(process, timeout):
    """One small query/second on our own child; no per-frame log or system polling."""
    psapi, kernel = ctypes.WinDLL('psapi'), ctypes.WinDLL('kernel32')
    psapi.GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.POINTER(ProcessMemory), ctypes.c_ulong]
    handle = ctypes.c_void_p(int(process._handle))
    started = time.monotonic()
    first = last = None
    peak = samples = 0
    while True:
        memory = ProcessMemory()
        memory.cb = ctypes.sizeof(memory)
        if psapi.GetProcessMemoryInfo(handle, ctypes.byref(memory), memory.cb):
            peak = max(peak, memory.peak_working)
            if time.monotonic() - started >= 10:
                last = {'working_bytes': memory.working, 'private_bytes': memory.private}
                if first is None:
                    first = last
                samples += 1
        try:
            code = process.wait(timeout=min(1, max(.01, timeout - (time.monotonic() - started))))
            break
        except subprocess.TimeoutExpired:
            if time.monotonic() - started >= timeout:
                process.kill()
                process.wait()
                raise
    times = [ctypes.c_ulonglong() for _ in range(4)]
    kernel.GetProcessTimes.argtypes = [ctypes.c_void_p] + [ctypes.POINTER(ctypes.c_ulonglong)] * 4
    cpu_seconds = None
    if kernel.GetProcessTimes(handle, *(ctypes.byref(value) for value in times)):
        cpu_seconds = (times[2].value + times[3].value) / 10000000
    return code, {'peak_working_bytes': peak, 'first_after_10s': first, 'last': last,
                  'memory_samples': samples, 'cpu_seconds': cpu_seconds,
                  'wall_seconds': time.monotonic() - started}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--priority', choices=('normal', 'above-normal'), default='normal',
                        help='Bounded scheduling diagnostic; changes only the child game process')
    parser.add_argument('--vsync', type=int, choices=(0, 1),
                        help='Override Vsync only in the isolated diagnostic configuration')
    parser.add_argument('--canonical-render-baseline', action='store_true',
                        help='Measure Player20 with rendering120; not a Player120 acceptance run')
    parser.add_argument('--profile', action='store_true',
                        help='Enable detailed CPU/GPU phase profiling; leave off for acceptance timing')
    parser.add_argument('--profile-cpu', action='store_true',
                        help='Enable CPU phase timings without inserting GPU timestamp queries')
    parser.add_argument('--cpu-counters', action='store_true',
                        help='Opt-in one-second CPU frequency/load counters for a shared slowdown')
    parser.add_argument('--shader-cache', type=Path,
                        help='Copy an existing compiler-output cache into this fresh run; never reuses saves')
    args = parser.parse_args()
    dest = args.output.resolve()
    if not dest.is_relative_to(ROOT / 'build'):
        raise ValueError('Output must be a fresh directory inside repository build/')
    dest.mkdir(parents=True, exist_ok=False)
    work, out = dest / 'work', dest / 'output'
    work.mkdir(); out.mkdir()
    source = ROOT / 'build/x64/soh'
    cache_source = args.shader_cache or source / 'cache/dx11-shaders-v1'
    cache_receipt = {}
    if cache_source.is_dir():
        cache_files = list(cache_source.glob('*.shc'))
        if sum(p.stat().st_size for p in cache_files) > 16 * 1024 * 1024:
            raise ValueError('Shader cache exceeds the engine 16-MB cap')
        cache_destination = work / 'cache/dx11-shaders-v1'
        cache_destination.mkdir(parents=True)
        for p in cache_files:
            shutil.copy2(p, cache_destination / p.name)
            cache_receipt[p.name] = hashlib.sha256(p.read_bytes()).hexdigest()
    config = json.loads((source / 'shipofharkinian.json').read_text())
    # Copy settings only, never saves. Automated input owns this fresh session.
    profile = 2 if args.profile_cpu else int(args.profile)
    config.setdefault('CVars', {}).setdefault('gDeveloperTools', {})['PlayerFrameProfile'] = profile
    if args.vsync is not None:
        config['CVars'].setdefault('gSettings', {})['VsyncEnabled'] = args.vsync
    (work / 'shipofharkinian.json').write_text(json.dumps(config, indent=2))
    fixture = json.loads((ROOT / 'scripts/native-simulation/fixtures/player-high-rate/next-frame-motion-120.json').read_text())
    fixture.update(id='player120-sustained-performance', ticks=1200, performance_only=1,
                   assertions=[], profile_player=profile)
    fixture.pop('expected_attack_edge_q', None)
    if args.canonical_render_baseline:
        fixture.update(id='canonical20-render120-performance-baseline', player_hz=20,
                       require_player_hz=False, verify_next_frame_response=False)
    # Full-stick running/turn/slash at20s and40s, with long natural idle/fidget intervals.
    original = fixture['input']
    fixture['input'] = [original[0]]
    for start in (2400, 4800):
        for original_event in original[1:]:
            event = dict(original_event)
            event['time_num'] += start
            event['sequence'] = len(fixture['input'])
            fixture['input'].append(event)
    fixture_path = dest / 'fixture.json'
    fixture_path.write_text(json.dumps(fixture, indent=2))
    links = []
    for name in ('oot.o2r', 'soh.o2r', 'gamecontrollerdb.txt'):
        os.link(source / name, work / name)
        links.append(work / name)
    exe = ROOT / 'x64/Release/soh.exe'
    source_diff = subprocess.check_output(['git', 'diff', 'HEAD'], cwd=ROOT)
    dependency_diff = subprocess.check_output(['git', 'diff', 'HEAD'], cwd=ROOT / 'libultraship')
    (dest / 'source.patch').write_bytes(source_diff)
    (dest / 'dependency.patch').write_bytes(dependency_diff)
    provenance = {'executable_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(),
                  'source': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  'dependency': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT / 'libultraship', text=True).strip(),
                  'source_patch_sha256': hashlib.sha256(source_diff).hexdigest(),
                  'dependency_patch_sha256': hashlib.sha256(dependency_diff).hexdigest(),
                  'source_untracked_sha256': untracked_source_hashes(ROOT),
                  'dependency_untracked_sha256': untracked_source_hashes(ROOT / 'libultraship'),
                  'configuration': config, 'timeout_seconds': 150, 'priority': args.priority,
                  'shader_cache_sha256': cache_receipt,
                  'canonical_render_baseline': args.canonical_render_baseline}
    (dest / 'provenance.json').write_text(json.dumps(provenance, indent=2))
    env = dict(os.environ, TEMP=str(ROOT / '.tmp'), TMP=str(ROOT / '.tmp'))
    creationflags = subprocess.ABOVE_NORMAL_PRIORITY_CLASS if args.priority == 'above-normal' else 0
    counters = None
    try:
        if args.cpu_counters:
            counters = subprocess.Popen(['typeperf.exe',
                r'\Processor Information(*)\% Processor Performance',
                r'\Processor Information(_Total)\% Processor Utility',
                r'\Processor Information(_Total)\% DPC Time',
                r'\Processor Information(_Total)\% Interrupt Time',
                r'\System\Processor Queue Length', '-si', '1', '-sc', '150', '-f', 'CSV',
                '-o', str(dest / 'cpu-counters.csv')], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                creationflags=subprocess.CREATE_NO_WINDOW)
        with (dest / 'engine.log').open('w') as log:
            with subprocess.Popen([str(exe), '--native-sim-test', str(fixture_path), '--output', str(out)],
                                  cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT,
                                  creationflags=creationflags) as process:
                exit_code, metrics = wait_with_process_metrics(process, 150)
            (dest / 'process-metrics.json').write_text(json.dumps(metrics, indent=2))
    finally:
        if counters is not None:
            if counters.poll() is None:
                counters.terminate()
            counters.wait(timeout=10)
        # Assets are unchanged hardlinks, not evidence; preserve all diagnostics on failure.
        for p in links:
            assert p.resolve().is_relative_to(work)
            p.unlink()
        (dest / 'cleanup.json').write_text(json.dumps({'removed_staged_hardlinks': [p.name for p in links]}))
    receipt = out / 'performance.json'
    if receipt.exists():
        report = json.loads(receipt.read_text())
        print(json.dumps({k: v for k, v in report.items() if k not in ('fixture', 'last_64_intervals_ms')}, indent=2))
    print('Engine exit:', exit_code)
    return exit_code


if __name__ == '__main__':
    sys.exit(main())
