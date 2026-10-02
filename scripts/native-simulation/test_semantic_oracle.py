"""Run: python -B -m unittest discover -s scripts/native-simulation -p test_*.py -v"""

from fractions import Fraction
import math
import unittest

import semantic_oracle as oracle


class SchedulingTests(unittest.TestCase):
    def test_supported_step_sizes_and_ratios(self):
        self.assertEqual([oracle.step_quanta(r) for r in oracle.SUPPORTED_RATES], [6, 4, 2, 1])
        self.assertEqual(oracle.step_scale(30), Fraction(2, 3))

    def test_invalid_rates_are_rejected(self):
        for rate in (0, -20, 40, 73, 144, 20.0, '20'):
            with self.assertRaises(ValueError):
                oracle.step_quanta(rate)

    def test_all_rates_meet_at_tenth_seconds(self):
        self.assertEqual(oracle.common_boundary_quanta(), 12)
        for rate in oracle.SUPPORTED_RATES:
            self.assertEqual(12 % oracle.step_quanta(rate), 0)
        self.assertNotEqual(6 % oracle.step_quanta(30), 0)

    def test_two_hours_of_integer_clock_has_no_drift(self):
        for rate in oracle.SUPPORTED_RATES:
            self.assertEqual(rate * 7200 * oracle.step_quanta(rate), 120 * 7200)

    def test_exact_boundary_is_not_delayed(self):
        for rate in oracle.SUPPORTED_RATES:
            self.assertEqual(oracle.next_boundary(Fraction(12), rate), 12)

    def test_arbitrary_input_timestamp_is_causal(self):
        # One nanosecond after 0.1 s is deliberately not on the 120-Hz lattice.
        event = Fraction(12) + Fraction(120, 1_000_000_000)
        for rate in oracle.SUPPORTED_RATES:
            applied = oracle.next_boundary(event, rate)
            self.assertGreaterEqual(applied, event)
            self.assertLess(applied - event, oracle.step_quanta(rate))

    def test_thirty_hz_deadline_jitter_does_not_accumulate(self):
        scheduled = oracle.periodic_schedule(1, 120, 30)
        self.assertEqual(scheduled[:4], [(6, 8), (12, 12), (18, 20), (24, 24)])
        self.assertEqual(len(scheduled), 20)
        self.assertEqual(scheduled[-1], (120, 120))

    def test_durations_keep_exact_deadline_separate_from_dispatch(self):
        for rate in oracle.SUPPORTED_RATES:
            for duration in (1, 2, 3, 5, 17, 255):
                due = duration * 6
                dispatch = oracle.next_boundary(Fraction(due), rate)
                self.assertGreaterEqual(dispatch, due)
                self.assertLess(dispatch - due, oracle.step_quanta(rate))

    def test_all_due_opportunities_are_accounted_for(self):
        for rate in oracle.SUPPORTED_RATES:
            for period in (1, 2, 3, 7):
                events = oracle.periodic_schedule(period, 1200, rate)
                self.assertEqual(len(events), 1200 // (6 * period))
                self.assertEqual([due for due, _ in events], list(range(6 * period, 1201, 6 * period)))

    def test_invalid_deadlines_and_periods_are_rejected(self):
        with self.assertRaises(ValueError):
            oracle.next_boundary(Fraction(-1), 30)
        for period in (0, -1, Fraction(1, 2)):
            with self.assertRaises(ValueError):
                oracle.periodic_schedule(period, 120, 30)


class TransformationTests(unittest.TestCase):
    def test_linear_motion_matches_at_common_boundaries(self):
        for increment in (Fraction(1, 7), -3, 5):
            for rate in oracle.SUPPORTED_RATES:
                self.assertEqual(oracle.linear_increment(increment, rate) * (rate // 10),
                                 Fraction(increment) * 2)

    def test_acceleration_legacy_operation_reference(self):
        self.assertEqual(oracle.affine_acceleration(10, 3, -1, 20, Fraction(3, 2)),
                         (Fraction(13), Fraction(2)))

    def test_affine_acceleration_matches_legacy_at_common_boundaries(self):
        for coefficient in (1, Fraction(3, 2)):
            for rate in oracle.SUPPORTED_RATES:
                x, v = Fraction(10), Fraction(3)
                for _ in range(rate):
                    x, v = oracle.affine_acceleration(x, v, Fraction(-1, 3), rate, coefficient)
                # Independent closed form for 20 old semi-implicit steps.
                expected_v = Fraction(3) - Fraction(20, 3)
                expected_x = 10 + coefficient * (20 * 3 - Fraction(20 * 21, 6))
                self.assertEqual((x, v), (expected_x, expected_v))

    def test_affine_acceleration_composes_at_thirty_hz(self):
        x, v = Fraction(0), Fraction(7)
        for _ in range(3):
            x, v = oracle.affine_acceleration(x, v, -2, 30)
        self.assertEqual((x, v), (Fraction(8), Fraction(3)))

    def test_naive_scaled_euler_does_not_match_legacy_endpoint(self):
        x, v = Fraction(0), Fraction(7)
        scale = oracle.step_scale(30)
        for _ in range(3):
            v += -2 * scale
            x += v * scale
        self.assertNotEqual(x, Fraction(8))

    def test_decay_preserves_real_time_ratio(self):
        for factor in (0.01, 0.5, 0.9, 1.0, 1.1):
            for rate in oracle.SUPPORTED_RATES:
                transformed = oracle.decay_factor(factor, rate)
                self.assertTrue(math.isclose(transformed ** (rate // 10), factor ** 2,
                                             rel_tol=2e-14, abs_tol=1e-14))

    def test_stationary_target_smoothing_preserves_error_ratio(self):
        for alpha in (0.01, 0.25, 0.75, 1.0):
            for rate in oracle.SUPPORTED_RATES:
                converted = oracle.smoothing_alpha(alpha, rate)
                self.assertTrue(math.isclose((1 - converted) ** (rate // 10), (1 - alpha) ** 2,
                                             rel_tol=2e-14, abs_tol=1e-14))

    def test_random_hazard_preserves_survival_not_seeded_path(self):
        for rate in oracle.SUPPORTED_RATES:
            p = oracle.opportunity_probability(0.2, rate)
            self.assertTrue(math.isclose((1 - p) ** rate, 0.8 ** 20, rel_tol=2e-14))

    def test_probability_endpoints(self):
        for rate in oracle.SUPPORTED_RATES:
            self.assertEqual(oracle.opportunity_probability(0.0, rate), 0.0)
            self.assertEqual(oracle.opportunity_probability(1.0, rate), 1.0)

    def test_very_small_probability_does_not_cancel_to_zero(self):
        p = oracle.opportunity_probability(1e-20, 120)
        self.assertGreater(p, 0)
        self.assertTrue(math.isclose(p, 1e-20 / 6, rel_tol=1e-14))

    def test_twenty_hz_returns_original_coefficients(self):
        # Python coefficient identity only, not game binary floating-point proof.
        for value in (0.0, 0.1, 0.7, 1.0):
            self.assertEqual(oracle.decay_factor(value, 20).hex(), value.hex())
            self.assertEqual(oracle.smoothing_alpha(value, 20).hex(), value.hex())

    def test_unsupported_exponential_models_fail_closed(self):
        for factor in (-0.5, float('nan'), float('inf')):
            with self.assertRaises(ValueError):
                oracle.decay_factor(factor, 30)
        for alpha in (-0.1, 1.1, float('nan')):
            with self.assertRaises(ValueError):
                oracle.smoothing_alpha(alpha, 60)


if __name__ == '__main__':
    unittest.main()
