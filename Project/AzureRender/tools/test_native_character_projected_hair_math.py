"""Analytic fixtures for the source-backed projected hair ramp contract."""
import math
import unittest

try:
    from endfield_native import projected_hair as subject
except ImportError:
    subject = None


class ProjectedHairMathTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(subject, 'Projected hair reference contract is missing')

    def coordinates(self, **updates):
        args = dict(half_vector=(1, 0, 0), highlight_normal=(0, 0, 1),
            view=(0, 0, 1), source_parameters=(1, .5, 0, .4),
            tangent=(1, 0, 0, 1), object_x=(1, 0, 0),
            object_y=(0, 1, 0), object_z=(0, 0, 1),
            primary_shift=0, secondary_shift=0, range2=.5,
            edge_fade_power=2, direction_x=0)
        args.update(updates)
        return subject.project_coordinates(**args)

    def assertVectorAlmostEqual(self, actual, expected, places=12):
        for a, e in zip(actual, expected):
            self.assertAlmostEqual(a, e, places=places)

    def test_two_dimensional_ramp_retains_sign_and_projected_edge(self):
        root = math.sqrt(.5)
        front = self.coordinates(half_vector=(root, root, 0),
            view=(math.sqrt(.75), 0, .5), source_parameters=(1, 1, 0, .4))
        back = self.coordinates(half_vector=(root, -root, 0),
            view=(math.sqrt(.75), 0, .5), source_parameters=(1, 1, 0, .4))
        self.assertAlmostEqual(front.edge_fade, .25)
        self.assertAlmostEqual(front.uv[0] / 2**-100, 1, places=11)
        self.assertAlmostEqual(front.uv[1], .0625)
        self.assertAlmostEqual(back.uv[0] / 2**-100, 1, places=11)
        self.assertEqual(back.uv[1], 0)

    def test_channels_keep_primary_and_secondary_energy_independent(self):
        coordinates = self.coordinates()
        energy = subject.evaluate_energy(coordinates, (1, .5, .25),
            (1, .5, 0, .4), 2, (.2, .4, .8))
        self.assertVectorAlmostEqual(energy.q, (.5, .25, .125))
        self.assertVectorAlmostEqual(energy.primary, (.1, .05, .025))
        self.assertVectorAlmostEqual(energy.secondary, (.04, .08, .16))
        empty = subject.evaluate_energy(self.coordinates(source_parameters=(1, 0, 0, .4)),
            (1, .5, .25), (1, 0, 0, .4), 2, (.2, .4, .8))
        self.assertVectorAlmostEqual(empty.primary, (0, 0, 0))
        self.assertVectorAlmostEqual(empty.secondary, (.08, .16, .32))
        no_secondary = subject.evaluate_energy(coordinates, (1, .5, .25),
            (1, .5, 0, 0), 2, (.2, .4, .8))
        self.assertVectorAlmostEqual(no_secondary.secondary, (0, 0, 0))

    def test_parameter_r_and_tangent_handedness_change_strand_sign(self):
        half = (math.sqrt(.5), math.sqrt(.5), 0)
        object_axis = self.coordinates(half_vector=half,
            source_parameters=(0, 1, 0, 1))
        mesh_axis = self.coordinates(half_vector=half,
            source_parameters=(1, 1, 0, 1))
        mirrored = self.coordinates(half_vector=half,
            source_parameters=(1, 1, 0, 1), tangent=(1, 0, 0, -1))
        self.assertEqual(object_axis.uv[1], 0)
        self.assertEqual(mesh_axis.uv[1], 1)
        self.assertEqual(mirrored.uv[1], 0)
        self.assertAlmostEqual(mesh_axis.uv[0] / 2**-100, 1, places=11)

    def test_secondary_range_uses_integer_exponent_and_signed_shift(self):
        half = (math.sqrt(.5), math.sqrt(.5), 0)
        truncated = self.coordinates(half_vector=half, range2=.496)
        self.assertAlmostEqual(truncated.secondary_lobe / 2**-50, 1, places=11)
        broad = self.coordinates(half_vector=half, range2=1.5)
        self.assertEqual(broad.secondary_lobe, 1)
        shifted = self.coordinates(half_vector=(0, 0, 1), primary_shift=1,
            secondary_shift=-1, source_parameters=(1, 1, 0, 1), range2=.992)
        self.assertAlmostEqual(shifted.uv[0] / 2**-100, 1, places=11)
        self.assertEqual(shifted.uv[1], 1)
        self.assertAlmostEqual(shifted.secondary_lobe, math.sqrt(.5))

    def test_object_basis_rotation_preserves_coordinates(self):
        root = math.sqrt(.5)
        baseline = self.coordinates(half_vector=(root, root, 0),
            view=(math.sqrt(.75), 0, .5))
        rotated = self.coordinates(half_vector=(0, root, -root),
            highlight_normal=(1, 0, 0), view=(.5, 0, -math.sqrt(.75)),
            tangent=(0, 0, -1, 1), object_x=(0, 0, -1),
            object_y=(0, 1, 0), object_z=(1, 0, 0))
        self.assertVectorAlmostEqual(rotated.uv, baseline.uv)
        self.assertAlmostEqual(rotated.edge_fade, baseline.edge_fade)
        self.assertAlmostEqual(rotated.secondary_lobe, baseline.secondary_lobe)

    def test_degenerate_projection_and_strand_are_finite(self):
        projected = self.coordinates(view=(0, 1, 0))
        self.assertEqual(projected.edge_fade, 0)
        self.assertEqual(projected.uv[1], 0)
        cancelled = self.coordinates(source_parameters=(.5, 1, 0, 1),
            half_vector=(0, 0, 0), highlight_normal=(0, 0, 0),
            object_x=(0, 0, 0), object_y=(0, 0, 0), object_z=(0, 0, 0))
        for value in (*cancelled.uv, cancelled.edge_fade, cancelled.secondary_lobe):
            self.assertTrue(math.isfinite(value))


if __name__ == '__main__':
    unittest.main()
