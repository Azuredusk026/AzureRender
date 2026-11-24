import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from audit_experiment import verify_schedule


class ScheduleTests(unittest.TestCase):
    def setUp(self):
        paths = ['traditional', 'subpasses', 'drlr']
        self.plan = dict(frames=1000, blocks=[dict(block=i, paths=paths) for i in range(1, 41)])
        self.rows = [dict(block=block['block'], position=i, path=path)
                     for block in self.plan['blocks'] for i, path in enumerate(block['paths'], 1)]
        self.index = dict(status='running', planSha256='plan', accepted=self.rows[:90], rejected=[])
        self.pause = dict(status='paused', planSha256='plan', acceptedRuns=90, completedBlocks=30,
                          measuredFrames=90000, rejectedBlocks=0, nextBlock=31, remainingRuns=30)

    def test_checkpoint_requires_the_exact_accepted_prefix(self):
        self.assertEqual(verify_schedule(self.plan, self.index, self.pause), 30)
        self.index['accepted'][0], self.index['accepted'][1] = self.index['accepted'][1], self.index['accepted'][0]
        with self.assertRaises(ValueError):
            verify_schedule(self.plan, self.index, self.pause)

    def test_checkpoint_rejects_inconsistent_receipt_or_incomplete_block(self):
        with self.assertRaises(ValueError):
            verify_schedule(self.plan, self.index, dict(self.pause, nextBlock=32))
        self.index['accepted'].pop()
        with self.assertRaises(ValueError):
            verify_schedule(self.plan, self.index, self.pause)

    def test_formal_output_requires_complete_schedule(self):
        with self.assertRaises(ValueError):
            verify_schedule(self.plan, self.index, None)
        full_index = dict(self.index, status='passed', accepted=self.rows)
        self.assertEqual(verify_schedule(self.plan, full_index, None), 40)
