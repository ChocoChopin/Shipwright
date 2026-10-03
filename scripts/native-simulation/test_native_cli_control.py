"""Controller-only tests; no game process or deliberately induced native fault."""
from pathlib import Path
import unittest
from unittest.mock import patch

import validate_native_cli as cli


class NativeCliControlTests(unittest.TestCase):
    def test_complete_case_set_is_preserved(self):
        with patch.object(cli, "run_case", return_value={"status": "pass", "exit_code": 2}) as run:
            reports, requested = cli.execute_cases(Path("soh.exe"), Path("output"), 10)
        self.assertEqual(requested, 43)
        self.assertEqual(len(reports), requested)
        self.assertEqual(run.call_count, requested)

    def test_first_unexpected_result_stops_case_dispatch(self):
        for failure in ({"status": "fail", "exit_code": 0xE06D7363},
                        {"status": "fail", "error": "timeout"},
                        {"status": "fail", "exit_code": 2}):
            with self.subTest(failure=failure), patch.object(
                    cli, "run_case", side_effect=[{"status": "pass"}, failure]) as run:
                reports, requested = cli.execute_cases(Path("soh.exe"), Path("output"), 10)
                self.assertEqual(run.call_count, 2)
                self.assertEqual(reports[-1], failure)
                self.assertEqual(requested, 43)


if __name__ == "__main__":
    unittest.main()
