import unittest

try:
    from endfield_native.spline_motion import advance_emission, sample_distance, constant_timing
except ImportError:
    advance_emission = sample_distance = constant_timing = None


def segment(points, length):
    return {'controls': points, 'stored_length': length}


class SplineMotionTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(advance_emission, 'source motion evaluator is missing')

    def test_frame_crossing_emits_with_age_and_keeps_remainder(self):
        ages, remainder = advance_emission(.25, 1.0, 2.0, 2.0)
        self.assertEqual(ages, [1.75, 1.25, .75, .25])
        self.assertEqual(remainder, .25)

    def test_paused_clock_preserves_pending_fraction(self):
        self.assertEqual(advance_emission(.25, 10, 0, 2), ([], .25))

    def test_bezier_parameter_uses_stored_length_not_arc_length(self):
        path = [segment([[0, 0, 0], [0, 0, 0], [0, 4, 0], [0, 4, 0]], 4)]
        self.assertEqual(sample_distance(path, 1), [0, .625, 0])
        self.assertEqual(sample_distance(path, 2), [0, 2, 0])

    def test_boundary_moves_to_next_slot_and_total_length_expires(self):
        path = [segment([[0, 0, 0]] * 4, 2), segment([[3, 0, 0]] * 4, 1)]
        self.assertEqual(sample_distance(path, 2), [3, 0, 0])
        self.assertIsNone(sample_distance(path, 3))

    def test_tiny_terminal_slot_is_retained(self):
        path = [segment([[0, 0, 0]] * 4, 4), segment([[1, 0, 0]] * 4, .001)]
        self.assertEqual(sample_distance(path, 4.0005), [1, 0, 0])

    def test_lifetime_clamps_source_fraction_and_scales_with_path(self):
        self.assertEqual(constant_timing(10, 2, 5, .5, 2),
                         {'simulation_lifetime': 5, 'wall_lifetime': 2.5,
                          'wall_emission_interval': 1, 'prewarm_count': 3})
        self.assertEqual(constant_timing(10, 2, .5, 1, 1)['simulation_lifetime'], 2.5)

    def test_invalid_geometry_and_rates_are_rejected(self):
        for rate in (-1, float('nan'), float('inf')):
            with self.assertRaises(ValueError):
                advance_emission(0, 1, 1, rate)
        with self.assertRaises(ValueError):
            sample_distance([segment([[0, 0, 0]] * 4, 0)], 0)
        with self.assertRaises(ValueError):
            sample_distance([segment([[0, 0, 0]] * 3, 1)], 0)
        with self.assertRaises(ValueError):
            constant_timing(10, 0, 1, 1, 1)

    def test_emission_work_is_bounded(self):
        with self.assertRaises(ValueError):
            advance_emission(0, 1e10, 1, 1)


if __name__ == '__main__':
    unittest.main()
