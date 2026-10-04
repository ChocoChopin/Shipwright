import copy
import unittest
import analyze_purity as purity


class PurityAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.fixture = {"id": "plain", "message_text_id": 0x1043}
        self.result = {"status": "pass", "extra_calls": 2, "fixture": self.fixture,
                       "packet_bytes_checked": True,
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
                             ("admission_negatives", {}), ("fixture", {}), ("packet_bytes_checked", False)):
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

    def test_player_requires_complete_measured_extraction_and_negative_admission(self):
        self.fixture.update(observe_player_state=True, ticks=4)
        with self.assertRaises(purity.replay.ReplayError):
            purity.validate_purity(self.result, self.fixture, "tested")
        self.result["coverage"]["player"] = dict(setup=1, measured=4, visible=5, invisible=0, commands=100)
        self.result["admission_negatives"]["player"] = 13
        purity.validate_purity(self.result, self.fixture, "tested")
        for measured, negatives in ((3, 13), (0, 13), (4, 12)):
            with self.subTest(measured=measured, negatives=negatives):
                self.result["coverage"]["player"].update(measured=measured, visible=measured + 1)
                self.result["admission_negatives"]["player"] = negatives
                with self.assertRaises(purity.replay.ReplayError):
                    purity.validate_purity(self.result, self.fixture, "tested")


if __name__ == "__main__":
    unittest.main()
