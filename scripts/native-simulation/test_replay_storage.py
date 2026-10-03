"""Storage lifecycle checks with tiny local files and a mocked process boundary."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import run_corpus as replay


class ReplayStorageTests(unittest.TestCase):
    def setUp(self):
        base = replay.ROOT / '.test-tmp/replay-storage'
        base.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=base)
        self.root = Path(self.temp.name)
        self.source = self.root / 'original.bin'
        self.source.write_bytes(b'original input')
        self.assets = {'asset.bin': self.source}
        self.work = self.root / 'work'
        self.work.mkdir()

    def tearDown(self):
        self.temp.cleanup()

    def test_identical_copy_released_but_source_and_evidence_preserved(self):
        shutil.copyfile(self.source, self.work / 'asset.bin')
        evidence = self.work / 'process.log'
        evidence.write_text('retained')
        self.assertEqual(replay.release_staged_assets(self.work, self.assets), ['asset.bin'])
        self.assertEqual(self.source.read_bytes(), b'original input')
        self.assertTrue(evidence.exists())
        self.assertFalse((self.work / 'asset.bin').exists())

    def test_hardlink_released_without_removing_original(self):
        (self.work / 'asset.bin').hardlink_to(self.source)
        replay.release_staged_assets(self.work, self.assets)
        self.assertEqual(self.source.read_bytes(), b'original input')

    def test_changed_copy_preserves_all_staged_inputs(self):
        shutil.copyfile(self.source, self.work / 'asset.bin')
        (self.work / 'changed.bin').write_bytes(b'changed')
        with self.assertRaisesRegex(replay.ReplayError, 'preserving all'):
            replay.release_staged_assets(self.work, {**self.assets, 'changed.bin': self.source})
        self.assertTrue((self.work / 'asset.bin').exists())
        self.assertTrue((self.work / 'changed.bin').exists())

    def test_original_and_parent_paths_rejected(self):
        for work, assets in [(self.root, {'original.bin': self.source}),
                             (self.work, {'../original.bin': self.source})]:
            with self.subTest(assets=assets), self.assertRaises(replay.ReplayError):
                replay.release_staged_assets(work, assets)
        self.assertTrue(self.source.exists())

    def test_free_space_includes_copy_budget_and_evidence_reserve(self):
        with patch.object(replay.shutil, 'disk_usage') as usage:
            usage.return_value.free = 1024 ** 3 + 10
            replay.require_replay_space(self.root, 10)
            with self.assertRaisesRegex(replay.ReplayError, 'storage preflight'):
                replay.require_replay_space(self.root, 11)

    def test_low_space_prevents_process_start(self):
        with patch.object(replay.shutil, 'disk_usage') as usage, patch.object(replay.subprocess, 'run') as run:
            usage.return_value.free = 0
            with self.assertRaises(replay.ReplayError):
                replay.launch(Path('unused.exe'), Path('unused.json'), self.root / 'attempt', self.assets, 1, False)
            run.assert_not_called()

    def test_failed_process_preserves_staged_assets(self):
        fixture = self.root / 'fixture.json'
        fixture.write_text('{}')
        with patch.object(replay.subprocess, 'run', return_value=subprocess.CompletedProcess([], 2)):
            with self.assertRaisesRegex(replay.ReplayError, 'Engine run failed'):
                replay.launch(Path('unused.exe'), fixture, self.root / 'attempt', self.assets, 1, False)
        self.assertTrue((self.root / 'attempt/work/asset.bin').exists())


if __name__ == '__main__':
    unittest.main()
