"""Asset-free native/Python parity for the compact validation implementation."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess
from run_corpus import fixture_assertions, write_json
from semantic_checkpoints import semantic_hash


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    specs = [dict(kind='changes', field='counter', minimum_distinct=2),
             dict(kind='distance', field='player.position', minimum=2),
             dict(kind='range', field='counter', minimum_span=2),
             dict(kind='decreases', field='player.position.y.value', minimum_drop=2),
             dict(kind='at_least', field='counter', minimum=3),
             dict(kind='counter_advance', field='counter', amount=2),
             dict(kind='ever_bits', field='counter', mask=2),
             dict(kind='ever_nonzero', field='counter'),
             dict(kind='input', tick=1, pressed=4),
             dict(kind='fall_landing', minimum_drop=2)]
    rows = []
    for tick in range(3):
        vec = lambda y: dict(x=dict(value=0.0), y=dict(value=y), z=dict(value=0.0))
        rows.append(dict(schema=1, tick=tick, time_q=tick*6, counter=tick+1,
                         player=dict(action='unit', position=vec(4.0-2*tick), velocity=vec(-2.0), bg_flags=int(tick==2)),
                         actors=[], input=dict(pressed=4 if tick==1 else 0),
                         types=[None, False, -1, 2**64-1, -0.0, 0.0, 'unicode é', {}, []]))
    cases = []
    # Each kind both passes and fails; parity is against the existing assertions.
    for spec in specs:
        fixture = dict(id='unit', ticks=2, assertions=[spec])
        cases.append(dict(fixture=fixture, rows=rows))
        bad = copy.deepcopy(spec)
        if bad['kind'] == 'input':
            bad['pressed'] = 9
        elif bad['kind'] == 'ever_nonzero':
            bad['field'] = 'time_q'
        else:
            for key in ('minimum', 'minimum_span', 'minimum_drop', 'minimum_distinct', 'amount', 'mask'):
                if key in bad:
                    bad[key] = 999
        bad_rows = copy.deepcopy(rows)
        if bad['kind'] == 'ever_nonzero':
            bad['field'] = 'zero'
            for row in bad_rows:
                row['zero'] = 0
        cases.append(dict(fixture=dict(id='unit', ticks=2, assertions=[bad]), rows=bad_rows))
    for threshold in (-1, 0):
        cases.append(dict(fixture=dict(id='unit', ticks=2,
            assertions=[dict(kind='changes',field='counter',minimum_distinct=threshold)]), rows=rows))
    path = (args.output/'cases.json').resolve()
    write_json(path, dict(cases=cases))
    result = subprocess.run([str(args.exe.resolve()), '--native-validation-unit-test', str(path)],
                            capture_output=True, text=True, timeout=30, check=True)
    native = json.loads((args.output/"native-result.json").read_text())
    assert native['sha_empty'] == hashlib.sha256(b'').hexdigest()
    assert native['sha_abc'] == hashlib.sha256(b'abc').hexdigest()
    assert native['ring'] == [96, 97, 98, 99]
    for case, actual in zip(cases, native['cases'], strict=True):
        assert actual['hashes'] == [semantic_hash(row) for row in case['rows']]
        expected = fixture_assertions(case['fixture'], case['rows'])
        assert actual['assertions']['status'] == expected['status'], case['fixture']
    for flags in (["--reference-checkpoints"], ["--diagnostic-snapshots"]):
        denied = subprocess.run([str(args.exe.resolve()), *flags], capture_output=True, timeout=30)
        assert denied.returncode == 2, flags
    write_json(args.output/'result.json', dict(status='pass', cases=len(cases), cli_controls=2,
        typed_hash_comparisons=sum(len(c['rows']) for c in cases), assertion_kinds=10,
        sha256_known_vectors=2, ring_overwrite_order='pass'))
    print(f'Online validation: {len(cases)} assertion cases, {len(cases)*3} typed hashes, 2 SHA vectors, bounded ring and CLI PASS')


if __name__ == '__main__':
    main()
