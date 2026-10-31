import unittest

from play_scenario import scenario_config, scenarios


class ScenarioConfigurationTests(unittest.TestCase):
    def test_catalog_preserves_authored_entry_or_explicit_position(self):
        for row in scenarios():
            with self.subTest(row=row["name"]):
                cfg = scenario_config({}, row, 120)
                self.assertEqual(cfg["CVars"]["gDeveloperTools"]["NativePlaytestEntranceOnly"],
                                 int("position" not in row))
                self.assertTrue(cfg["WarpPoints"][row["name"]]["bootToPoint"])
                self.assertTrue(row["actions"])

    def test_comparison_changes_player_cadence_only(self):
        for row in scenarios():
            high = scenario_config({}, row, 120)
            original = scenario_config({}, row, 20)
            high["CVars"]["gDeveloperTools"]["NativePlayerHz"] = 20
            self.assertEqual(high, original)

    def test_preferences_are_not_mutated_or_replaced(self):
        seed = {"Window": {"Fullscreen": True}, "CVars": {"gSettings": {"VsyncEnabled": 1}}}
        cfg = scenario_config(seed, scenarios()[0], 120)
        self.assertEqual(seed, {"Window": {"Fullscreen": True}, "CVars": {"gSettings": {"VsyncEnabled": 1}}})
        self.assertEqual(cfg["Window"], seed["Window"])
        self.assertEqual(cfg["CVars"]["gSettings"]["VsyncEnabled"], 1)

    def test_only_horse_requests_mounted_start(self):
        for row in scenarios():
            cfg = scenario_config({}, row, 120)
            self.assertEqual(cfg["CVars"]["gDeveloperTools"]["NativeHorseRideStart"],
                             2 if row["name"] == "horse" else 0)


if __name__ == "__main__":
    unittest.main()
