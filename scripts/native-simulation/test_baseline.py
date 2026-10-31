"""Build environment checks; no compiler, game process or global Git writes."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import baseline


class BaselineEnvironmentTests(unittest.TestCase):
    def test_inherited_configuration_is_preserved_without_global_mutation(self):
        inherited = {"GIT_CONFIG_COUNT": "1", "GIT_CONFIG_KEY_0": "color.ui",
                     "GIT_CONFIG_VALUE_0": "never", "PATH": "unchanged"}
        with patch.dict(os.environ, inherited, clear=True):
            env = baseline.git_environment()
            self.assertEqual(dict(os.environ), inherited)
        self.assertEqual(env["PATH"], "unchanged")
        self.assertEqual(env["GIT_CONFIG_VALUE_0"], "never")
        self.assertEqual(env["GIT_CONFIG_COUNT"], "5")
        paths = [env[f"GIT_CONFIG_VALUE_{i}"] for i in range(1, 5)]
        self.assertEqual(paths, [p.resolve().as_posix() for p in
                         (baseline.ROOT, baseline.ROOT / "libultraship", baseline.ROOT / "torch",
                          baseline.BUILD / "vcpkg")])
        self.assertTrue(all(env[f"GIT_CONFIG_KEY_{i}"] == "safe.directory" for i in range(1, 5)))
        self.assertNotIn("*", paths)

    def test_bad_inherited_count_is_not_silently_discarded(self):
        for count in ("-1", "bad"):
            with self.subTest(count=count), patch.dict(os.environ, {"GIT_CONFIG_COUNT": count}):
                with self.assertRaisesRegex(RuntimeError, "GIT_CONFIG_COUNT"):
                    baseline.git_environment()

    def test_dependency_outside_checkout_is_not_trusted(self):
        with patch.object(baseline, "BUILD", baseline.ROOT.parent / "unrelated"):
            with self.assertRaisesRegex(RuntimeError, "outside this checkout"):
                baseline.git_environment()

    def test_metadata_checks_and_build_child_all_receive_environment(self):
        base = (baseline.ROOT / ".test-tmp" / "baseline").resolve()
        self.assertTrue(base.is_relative_to(baseline.ROOT))
        base.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=base) as directory:
            self.assertTrue(Path(directory).resolve().is_relative_to(base))
            env = baseline.git_environment()
            env.update({name: str(base) for name in ("TEMP", "TMP", "VCPKG_DEFAULT_BINARY_CACHE",
                       "VCPKG_DOWNLOADS", "VCPKG_ROOT", "SHIPWRIGHT_VCPKG_NO_UPDATE")})
            def fake_run(command, **kwargs):
                self.assertIn("GIT_CONFIG_COUNT", kwargs["env"])
                binary = command[:3] == ["git", "diff", "--binary"]
                return subprocess.CompletedProcess(command, 0, b"" if binary else "")
            with patch.object(baseline, "EVIDENCE", Path(directory)), \
                 patch.object(baseline, "local_environment", return_value=env), \
                 patch.object(baseline.subprocess, "run", side_effect=fake_run) as child:
                baseline.ignored(base / "ignore-probe")
                self.assertEqual(baseline.run_logged("environment-test", ["fake-build"]), 0)
            self.assertTrue(any(call.args[0] == ["fake-build"] for call in child.call_args_list))


if __name__ == "__main__":
    unittest.main()
