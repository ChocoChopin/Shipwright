import copy
import unittest
import analyze_purity as purity


class PurityAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.fixture = {"id": "plain", "message_text_id": 0x1043}
        self.result = {"status": "pass", "extra_calls": 2, "fixture": self.fixture,
                       "negative_control": False, "first_failure": "",
                       "identity": {"executable_sha256": "tested"},
                       "coverage": {"message": {"setup": 2, "measured": 4, "visible": 5,
                                                "invisible": 1, "commands": 100}},
                       "admission_negatives": {"message": 13}}

    def test_valid(self):
        purity.validate_purity(self.result, self.fixture, "tested")

    def test_rejects_missing_repeat_identity_or_mutation_evidence(self):
        for field, value in (("extra_calls", 1), ("status", "fail"), ("negative_control", True),
                             ("first_failure", "live state mutated"), ("identity", {}),
                             ("admission_negatives", {}), ("fixture", {})):
            with self.subTest(field=field):
                result = copy.deepcopy(self.result)
                result[field] = value
                with self.assertRaises(purity.replay.ReplayError):
                    purity.validate_purity(result, self.fixture, "tested")

    def test_fade_must_remain_fallback(self):
        self.fixture["message_text_id"] = 0x305F
        with self.assertRaises(purity.replay.ReplayError):
            purity.validate_purity(self.result, self.fixture, "tested")

    def test_inconsistent_counts(self):
        self.result["coverage"]["message"]["measured"] = 3
        with self.assertRaises(purity.replay.ReplayError):
            purity.validate_purity(self.result, self.fixture, "tested")

    def test_negative_counts(self):
        self.result["coverage"]["message"]["commands"] = -1
        with self.assertRaises(purity.replay.ReplayError):
            purity.validate_purity(self.result, self.fixture, "tested")


if __name__ == "__main__":
    unittest.main()
