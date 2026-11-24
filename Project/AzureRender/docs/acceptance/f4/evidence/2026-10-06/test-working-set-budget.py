import unittest
from working_set_budget import working_set_growth

class WorkingSetBudgetTests(unittest.TestCase):
    def test_shutdown_release_is_not_growth(self):
        samples = [10, 40, 80, 120, 200, 200, 202, 202, 150, 30]
        self.assertAlmostEqual(working_set_growth(samples), .01)

    def test_sustained_growth_keeps_five_percent_gate(self):
        samples = [10, 40, 80, 120, 200, 200, 215, 230, 150, 30]
        self.assertGreater(working_set_growth(samples), .05)

    def test_insufficient_observations_rejected(self):
        with self.assertRaises(ValueError):
            working_set_growth([200, 200])

if __name__ == '__main__':
    unittest.main()
