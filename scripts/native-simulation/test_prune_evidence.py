import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import prune_evidence as prune


class PruneEvidenceTests(unittest.TestCase):
    def test_preserves_failures_receipts_and_selected_success(self):
        temporary = prune.ROOT / ".test-tmp"
        temporary.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temporary) as name:
            root = Path(name)
            build = root / "build"
            build.mkdir()
            with patch.object(prune, "ROOT", root), patch.object(prune, "BUILD", build):
                for label, status in (("success", "pass"), ("failure", "fail")):
                    corpus = build / label
                    output = corpus / "case/run-001/output"
                    output.mkdir(parents=True)
                    (output / "trace.jsonl").write_text("{}\n")
                    (output / "snapshots.jsonl").write_text("{}\n")
                    (output.parent / "invocation.json").write_text(json.dumps(dict(status=status)))
                    (corpus / "corpus_result.json").write_text(json.dumps(dict(status=status, fixtures=[dict(id="case", status=status)])))
                retained = build / "success/case/run-001/output/snapshots.jsonl"
                manifest = prune.plan([build / "success", build / "failure"], [retained])
                self.assertEqual(len(manifest["files"]), 1)
                prune.apply(manifest, build / "cleanup.json")
                self.assertTrue(retained.exists())
                self.assertTrue((build / "failure/case/run-001/output/trace.jsonl").exists())
                self.assertTrue((build / "success/corpus_result.json").exists())
                self.assertFalse((build / "success/case/run-001/output/trace.jsonl").exists())

    def test_rejects_outside_build(self):
        with self.assertRaises(ValueError):
            prune.checked(prune.ROOT / "AGENTS.md")


if __name__ == "__main__":
    unittest.main()
