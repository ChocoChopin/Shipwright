"""Graceful online mismatch control; never induces a native fault."""
import argparse
from pathlib import Path
from run_corpus import (SemanticMismatch, checkpoint_run, launch, read_json,
                        release_staged_assets, write_json)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--assets', required=True, type=Path)
    parser.add_argument('--reference', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    manifest, document = checkpoint_run(args.reference)
    if manifest['fixture'].get('player_hz') != 120 or manifest['ticks_completed'] < 6:
        parser.error('Use a completed Player120 fixture with at least six boundaries')
    args.output.mkdir(parents=True, exist_ok=False)
    fixture = (args.output/'fixture.json').resolve()
    reference = (args.output/'altered-reference.json').resolve()
    write_json(fixture, manifest['fixture'])
    document['snapshots'][6]['sha256'] = '0'*64
    write_json(reference, document)  # Only this private diagnostic copy is changed.
    assets = {name: (args.assets/name).resolve(strict=True)
              for name in ('oot.o2r', 'soh.o2r', 'gamecontrollerdb.txt')}
    run = (args.output/'run').resolve()
    try:
        launch(args.exe.resolve(), fixture, run, assets, 90, False, reference_checkpoints=reference)
    except SemanticMismatch:
        pass
    else:
        raise AssertionError('Deliberate reference mismatch escaped the online comparator')
    invocation = read_json(run/'invocation.json')
    assert invocation['exit_code'] == 2
    failure = read_json(run/'output/result.json')
    assert failure['tick'] == 6 and 'semantic hash mismatch' in failure['error']
    boundaries = (run/'output/failure-boundaries.jsonl').read_text().splitlines()
    steps = (run/'output/failure-player-steps.jsonl').read_text().splitlines()
    assert len(boundaries) == 4 and len(steps) == 32
    assert not (run/'output/snapshots.jsonl').exists()
    assert not (run/'output/player-steps.jsonl').exists()
    # Retain every failure output; remove only identical staged asset hardlinks.
    released = release_staged_assets(run/'work', assets)
    write_json(args.output/'control.json', dict(status='pass', native_exit=2, mismatch_tick=6,
        retained_boundaries=len(boundaries), retained_substeps=len(steps), released_staged_assets=released,
        details_bytes=sum(p.stat().st_size for p in (run/'output').iterdir() if p.is_file())))
    print('Online mismatch detected at boundary 6; 4 boundaries + 32 substeps retained; normal exit 2')


if __name__ == '__main__':
    main()
