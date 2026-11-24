"""Validate GPU counter aggregation without requiring RenderDoc."""
import math
import unittest
from native_render.renderdoc_profile import summarize,select_shader_events


class ProfileTests(unittest.TestCase):
    def test_bounds_shader_inspection_by_measured_cost(self):
        rows=[dict(event=i,seconds=i/1000,kind='draw',shader='x') for i in range(1,100)]
        self.assertEqual(select_shader_events(rows,2),{98,99})
        for limit in [0,513,True]:
            with self.assertRaises(ValueError):select_shader_events(rows,limit)

    def test_groups_only_observed_events(self):
        rows=[dict(event=4,seconds=.002,kind='dispatch',shader='ray'),
              dict(event=7,seconds=.001,kind='dispatch',shader='ray'),
              dict(event=9,seconds=0,kind='draw',shader='mesh')]
        result=summarize(rows)
        self.assertEqual(result['events'],3)
        self.assertAlmostEqual(result['sumEventMs'],3)
        self.assertEqual(result['groups'][0]['events'],2)
        self.assertAlmostEqual(result['groups'][0]['milliseconds'],3)
        self.assertEqual(result['groups'][0]['shader'],'ray')

    def test_rejects_invalid_or_empty_measurements(self):
        for rows in [[],[dict(event=1,seconds=math.nan,kind='draw',shader='x')],
                     [dict(event=1,seconds=-1,kind='draw',shader='x')],
                     [dict(event=1,seconds=1,kind='draw',shader='x')]*2]:
            with self.subTest(rows=rows),self.assertRaises(ValueError):summarize(rows)


if __name__=='__main__':unittest.main()
