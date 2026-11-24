"""Budget gates must reject writes before a task consumes its reserve."""
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import artifact_cleanup as cleanup


class BudgetTests(unittest.TestCase):
    def test_reviewed_list_deletes_only_selected_eligible_run(self):
        import os
        import json
        import time
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('selected', 'other'):
                path = root/'runs'/name
                path.mkdir(parents=True)
                marker=path/'.finished'
                marker.touch()
                expired=time.time()-30*86400
                os.utime(marker,(expired,expired))
                os.utime(path,(expired,expired))
            selected=root/'runs'/'selected'
            batch=root/'batch.json'
            batch.write_text(json.dumps([str(selected)]))
            with patch.object(cleanup,'BUILD',root), patch.object(cleanup,'ENGINE',root):
                with patch('sys.argv',['cleanup','--apply','--run-list',str(batch)]), patch.object(cleanup.subprocess,'check_output',side_effect=[str(root),b'']):
                    cleanup.main()
            self.assertFalse(selected.exists())
            self.assertTrue((root/'runs'/'other').exists())

    def test_reviewed_list_refuses_outside_target(self):
        import json
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            batch=root/'batch.json'
            batch.write_text(json.dumps([str(root)]))
            with patch.object(cleanup,'BUILD',root), patch.object(cleanup,'ENGINE',root):
                with patch('sys.argv',['cleanup','--apply','--run-list',str(batch)]), patch.object(cleanup.subprocess,'check_output',side_effect=[str(root),b'']):
                    with self.assertRaises(SystemExit):
                        cleanup.main()
            self.assertTrue(batch.exists())

    def test_apply_refuses_more_than_ten_targets_before_any_deletion(self):
        import os
        import time
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            runs = root/'runs'
            for index in range(11):
                path = runs/str(index)
                path.mkdir(parents=True)
                marker = path/'.finished'
                marker.touch()
                expired = time.time()-30*86400
                os.utime(marker, (expired, expired))
                os.utime(path, (expired, expired))
            with patch.object(cleanup, 'BUILD', root), patch.object(cleanup, 'ENGINE', root):
                with patch('sys.argv', ['cleanup', '--apply']), patch.object(cleanup.subprocess, 'check_output', side_effect=[str(root), b'']):
                    with self.assertRaises(SystemExit):
                        cleanup.main()
            self.assertEqual(len(list(runs.iterdir())), 11)

    def test_new_run_refuses_junction_output_root(self):
        import _winapi
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root/'build'
            outside = root/'outside'
            build.mkdir()
            outside.mkdir()
            _winapi.CreateJunction(str(outside), str(build/'runs'))
            with patch.object(cleanup, 'BUILD', build), patch('shutil.disk_usage', return_value=(1000*2**30, 500*2**30, 500*2**30)):
                with patch('sys.argv', ['cleanup', '--new-run', 'capture']):
                    with self.assertRaises(SystemExit):
                        cleanup.main()
            self.assertEqual(list(outside.iterdir()), [])

    def test_new_run_rejects_low_space_before_creating_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(cleanup, 'BUILD', root), patch('shutil.disk_usage', return_value=(1000 * 2**30, 968 * 2**30, 32 * 2**30)):
                with patch('sys.argv', ['cleanup', '--new-run', 'capture']):
                    with self.assertRaises(SystemExit) as result:
                        cleanup.main()
            self.assertNotEqual(result.exception.code, 0)
            self.assertEqual(list((root / 'runs').iterdir()), [])

    def test_budget_boundaries_and_diagnostic_reserve(self):
        from artifact_budget import evaluate
        self.assertFalse(evaluate(total=1000*2**30, free=101*2**30, used=0, planned=2*2**30)['allowed'])
        self.assertTrue(evaluate(total=1000*2**30, free=102*2**30, used=0, planned=2*2**30)['allowed'])
        self.assertFalse(evaluate(total=1000*2**30, free=100*2**30, used=2*2**30, planned=1)['allowed'])
        self.assertTrue(evaluate(total=1000*2**30, free=32*2**30, used=0, planned=64*2**20, diagnostic=True)['allowed'])
        self.assertFalse(evaluate(total=1000*2**30, free=32*2**30, used=0, planned=65*2**20, diagnostic=True)['allowed'])
        self.assertFalse(evaluate(total=1000*2**30, free=5*2**30, used=0, planned=1, diagnostic=True)['allowed'])
        with self.assertRaises(ValueError):
            evaluate(total=100, free=50, used=0, planned=-1)

    def test_existing_run_quota_and_link_boundary(self):
        from artifact_budget import check
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            run = build/'runs'/'sample'
            run.mkdir(parents=True)
            (run/'data').write_bytes(b'1234')
            with patch('shutil.disk_usage', return_value=(1000*2**30, 500*2**30, 500*2**30)):
                self.assertFalse(check(build, run=run, planned=1, quota=4)['allowed'])
                with self.assertRaises(ValueError):
                    check(build, run=build, planned=0)
                import _winapi
                junction = build/'runs'/'linked'
                _winapi.CreateJunction(str(run), str(junction))
                with self.assertRaises(ValueError):
                    check(build, run=junction, planned=0)

    def test_diagnostic_budget_counts_existing_artifacts(self):
        from artifact_budget import evaluate
        result = evaluate(total=1000*2**30, free=32*2**30,
                          used=63*2**20, planned=2*2**20, diagnostic=True)
        self.assertFalse(result['allowed'])
        self.assertIn('diagnostic-output-limit', result['reasons'])
        exact = evaluate(total=1000*2**30, free=32*2**30,
                         used=32*2**20, planned=32*2**20, diagnostic=True)
        self.assertTrue(exact['allowed'])
        next_write = evaluate(total=1000*2**30, free=32*2**30,
                              used=64*2**20, planned=1, diagnostic=True)
        self.assertFalse(next_write['allowed'])

    def test_normal_budget_keeps_two_gibibyte_contract(self):
        from artifact_budget import evaluate
        result = evaluate(total=1000*2**30, free=500*2**30,
                          used=64*2**20, planned=64*2**20)
        self.assertTrue(result['allowed'])


if __name__ == '__main__':
    unittest.main()
