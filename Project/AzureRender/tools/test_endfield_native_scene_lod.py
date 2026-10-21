"""Catch wrong interval direction, missing base parts and shadow duplication."""
import unittest

try:
    from endfield_native.scene_lod import select_draw_indices
except ImportError:
    select_draw_indices = None


def pair(mesh, lower, upper, submesh=0, flags=0):
    return dict(mesh=mesh, material='surface', draw=dict(submesh=submesh,
                flags=flags, lod_min_squared=lower, lod_max_squared=upper))


class SceneLodTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(callable(select_draw_indices), 'source LOD selection required')
        self.parts = [pair('fine', .09, 3.4e38, 0),
                      pair('fine', .09, 3.4e38, 1),
                      pair('medium', .0225, .09),
                      pair('coarse', -.0001, .0225)]

    def test_metric_zero_selects_coarse_and_boundary_selects_upper_interval(self):
        self.assertEqual(select_draw_indices(self.parts), [3])
        self.assertEqual(select_draw_indices(self.parts, metric_squared=.09), [0, 1])
        self.assertEqual(select_draw_indices(self.parts, metric_squared=.0225), [2])

    def test_highest_keeps_all_base_submeshes_without_assuming_names(self):
        self.assertEqual(select_draw_indices(self.parts, policy='highest'), [0, 1])
        parts = self.parts + [pair('fine-second-part', .09, 3.4e38)]
        self.assertEqual(select_draw_indices(parts, policy='highest'), [0, 1, 4])

    def test_authored_single_mesh_can_use_any_lod_suffix(self):
        self.assertEqual(select_draw_indices([pair('single_lod3', -.001, 100)],
                                            policy='highest'), [0])

    def test_shadow_only_draw_is_excluded_before_selecting_interval(self):
        parts = self.parts + [pair('shadow', .5, 3.4e38, flags=1024)]
        self.assertEqual(select_draw_indices(parts, policy='highest'), [0, 1])
        self.assertEqual(select_draw_indices(parts), [3])

    def test_ambiguous_overlapping_highest_parts_are_rejected(self):
        parts = self.parts + [pair('independent-group', .2, 3.4e38)]
        with self.assertRaisesRegex(ValueError, 'overlap'):
            select_draw_indices(parts, policy='highest')

    def test_malformed_draws_are_rejected_before_any_selection(self):
        for bad in [pair('bad', float('nan'), 1), pair('bad', 2, 1),
                    pair('bad', 0, float('inf')), pair('bad', 0, 1, -1),
                    pair('bad', 0, 1, flags=-1), pair('', 0, 1)]:
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                select_draw_indices(self.parts + [bad], policy='highest')

    def test_invalid_policy_and_metric_are_rejected(self):
        for metric in [-1, float('nan'), float('inf'), True]:
            with self.subTest(metric=metric), self.assertRaises(ValueError):
                select_draw_indices(self.parts, metric_squared=metric)
        with self.assertRaises(ValueError):select_draw_indices(self.parts, policy='guess')

    def test_missing_visible_interval_is_reported(self):
        with self.assertRaisesRegex(ValueError, 'visible'):
            select_draw_indices([pair('shadow', 0, 1, flags=1024)], policy='highest')
        with self.assertRaisesRegex(ValueError, 'visible'):
            select_draw_indices(self.parts, metric_squared=3.4e38)


if __name__ == '__main__':unittest.main()
