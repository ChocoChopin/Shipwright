import copy
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import run_corpus as runner
from semantic_checkpoints import semantic_hash, FORMAT


class CheckpointTests(unittest.TestCase):
    def test_typed_hash_preserves_exact_semantics(self):
        values = [None, False, 0, 0.0, -0.0, '0', [], {}, [0], {'x': 0}, -1, 2**64-1]
        self.assertEqual(len(values), len({semantic_hash(v) for v in values}))
        self.assertEqual(semantic_hash({'b': 2, 'a': 1}), semantic_hash({'a': 1, 'b': 2}))
        with self.assertRaises(ValueError):
            semantic_hash(float('nan'))

    def test_compact_comparison_never_loads_snapshots(self):
        temporary = runner.ROOT / '.test-tmp'
        temporary.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temporary) as name:
            root = Path(name)
            manifest = dict(schema=1, status='pass', rate_hz=20, ticks_completed=1, fixture_id='unit', fixture={})
            document = dict(format=FORMAT, fixture={}, snapshots=[
                dict(tick=i, time_q=i*6, sha256=semantic_hash({'tick': i})) for i in range(2)])
            for label in ('a', 'b'):
                runner.write_json(root/label/'result.json', manifest)
                runner.write_json(root/label/'checkpoints.json', document)
            with patch.object(runner, 'load_run', side_effect=AssertionError('full snapshot load')):
                self.assertEqual(runner.compare_runs(root/'a', root/'b')['status'], 'pass')
                changed = copy.deepcopy(document)
                changed['snapshots'][1]['sha256'] = '0'*64
                runner.write_json(root/'b'/'checkpoints.json', changed)
                report = runner.compare_runs(root/'a', root/'b')
                self.assertEqual((report['status'], report['tick']), ('mismatch', 1))
                changed['snapshots'].pop()
                runner.write_json(root/'b'/'checkpoints.json', changed)
                with self.assertRaises(runner.ReplayError):
                    runner.compare_runs(root/'a', root/'b')
