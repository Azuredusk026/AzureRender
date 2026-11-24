"""Reject false completion, dependency drift and stale status mirrors."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).with_name('check_stage_status.py')


class StageStatusTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'evidence.md').write_text('# Evidence\n', encoding='utf-8')
        self.data = {
            'schemaVersion': 1,
            'stages': [
                {'id': 'UI8', 'name': '交付检查', 'status': 'Complete',
                 'dependsOn': [], 'checksPassed': True, 'evidence': ['evidence.md']},
                {'id': 'P8', 'name': '最终验收', 'status': 'Ready',
                 'dependsOn': ['UI8'], 'checksPassed': False, 'evidence': ['evidence.md'],
                 'requiredAcceptance': ['input']},
            ],
            'acceptance': [
                {'id': 'input', 'originPhase': 'UI8', 'ownerPhase': 'P8',
                 'result': 'pending', 'transferEvidence': 'evidence.md'},
            ],
            'mirrors': [],
        }

    def run_check(self, *args):
        self.assertTrue(TOOL.is_file(), 'Stage status validation tool is missing')
        (self.root / 'status.json').write_text(json.dumps(self.data), encoding='utf-8')
        return subprocess.run(
            [sys.executable, str(TOOL), '--root', str(self.root),
             '--manifest', 'status.json', '--page', 'status.md', *args],
            capture_output=True, encoding='utf-8')

    def test_transferred_gate_preserves_completed_implementation(self):
        self.assertEqual(self.run_check('--write-page').returncode, 0)

    def test_unknown_stage_state_is_rejected(self):
        self.data['stages'][0]['status'] = 'AutomaticMostlyVerified'
        self.assertNotEqual(self.run_check('--write-page').returncode, 0)

    def test_final_stage_cannot_complete_with_pending_gate(self):
        self.data['stages'][1].update(status='Complete', checksPassed=True)
        self.assertNotEqual(self.run_check('--write-page').returncode, 0)

    def test_completion_requires_passed_stage_checks(self):
        self.data['stages'][0]['checksPassed'] = False
        self.assertNotEqual(self.run_check('--write-page').returncode, 0)

    def test_stage_cannot_complete_before_its_required_transferred_gate(self):
        self.data['stages'][0]['requiredAcceptance'] = ['input']
        self.assertNotEqual(self.run_check('--write-page').returncode, 0)

    def test_transfer_requires_evidence(self):
        del self.data['acceptance'][0]['transferEvidence']
        self.assertNotEqual(self.run_check('--write-page').returncode, 0)

    def test_multiple_active_stages_are_rejected(self):
        for stage in self.data['stages']:
            stage['status'] = 'Active'
        self.assertNotEqual(self.run_check('--write-page').returncode, 0)

    def test_ready_requires_completed_dependencies(self):
        self.data['stages'][0].update(status='Pending', checksPassed=False)
        self.assertNotEqual(self.run_check('--write-page').returncode, 0)

    def test_cycle_is_rejected(self):
        self.data['stages'][0]['dependsOn'] = ['P8']
        self.assertNotEqual(self.run_check('--write-page').returncode, 0)

    def test_missing_evidence_is_rejected(self):
        self.data['stages'][0]['evidence'] = ['missing.md']
        self.assertNotEqual(self.run_check('--write-page').returncode, 0)

    def test_mirror_drift_is_rejected(self):
        (self.root / 'mirror.json').write_text(json.dumps({
            'currentStageStatuses': {'UI8': 'Active'}}), encoding='utf-8')
        self.data['mirrors'] = ['mirror.json']
        self.assertNotEqual(self.run_check('--write-page').returncode, 0)

    def test_missing_gate_from_source_is_rejected(self):
        (self.root / 'source.json').write_text(json.dumps({
            'items': [{'id': 'input'}, {'id': 'audio'}]}), encoding='utf-8')
        self.data['acceptanceSources'] = ['source.json']
        self.assertNotEqual(self.run_check('--write-page').returncode, 0)

    def test_owner_must_reference_every_owned_gate(self):
        self.data['stages'][1]['requiredAcceptance'] = []
        self.assertNotEqual(self.run_check('--write-page').returncode, 0)

    def test_stale_page_is_rejected(self):
        self.assertEqual(self.run_check('--write-page').returncode, 0)
        (self.root / 'status.md').write_text('# Stale\n', encoding='utf-8')
        self.assertNotEqual(self.run_check().returncode, 0)

    def test_project_is_ready_when_only_acceptance_remains(self):
        self.assertEqual(self.run_check('--write-page').returncode, 0)
        self.assertIn('项目状态：Ready。', (self.root / 'status.md').read_text(encoding='utf-8'))

    def test_project_reports_active_work(self):
        self.data['stages'][1]['status'] = 'Active'
        self.assertEqual(self.run_check('--write-page').returncode, 0)
        self.assertIn('项目状态：开发中。', (self.root / 'status.md').read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main()
