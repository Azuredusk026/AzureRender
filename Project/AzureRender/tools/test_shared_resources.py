"""Exercise actual process exclusion and main-index input protection."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

import artifact_maintenance as maintenance
import shared_resources as shared


class SharedResourcesTests(unittest.TestCase):
    def setUp(self):
        root = Path(__file__).resolve().parents[1]/'build/maintenance/tests'
        root.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=root)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root/'build'
        (self.build/'runs').mkdir(parents=True)

    def test_process_lock_excludes_other_streams_and_releases_after_exception(self):
        with self.assertRaisesRegex(RuntimeError, 'candidate failed'):
            with shared.lease(self.root/'queue', 'performance', 'compile', self.root):
                command = [sys.executable, '-c',
                    "from pathlib import Path; import shared_resources as s; "
                    "s.try_once(Path(__import__('sys').argv[1]), 'quality', 'gpu', Path('.'))",
                    str(self.root/'queue')]
                result = subprocess.run(command, cwd=Path(__file__).parent, capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b'busy', result.stderr)
                raise RuntimeError('candidate failed')
        with shared.lease(self.root/'queue', 'thesis', 'measure', self.root):
            value = json.loads((self.root/'queue/current.json').read_text())
            self.assertEqual(value['owner'], 'thesis')
            self.assertEqual(value['pid'], os.getpid())
        self.assertEqual(json.loads((self.root/'queue/current.json').read_text())['state'], 'released')

    def test_external_consumer_protects_input_and_release_checks_owner(self):
        source = self.build/'runs'/'source'
        maintenance.create_run(self.build, source, dict(plannedBytes=0))
        (source/'result.json').write_text('{}')
        maintenance.seal(self.build, source)
        (source/'.finished').touch()
        consumer = self.root/'other-worktree/run'
        consumer.mkdir(parents=True)
        token = shared.acquire_inputs(self.build, consumer, ['source'], 'quality')
        args = dict(apply=True, now=time.time()+2*86400, busy=[], tracked=set(), git=lambda:'same')
        maintenance.maintain(self.build, **args)
        self.assertTrue(source.exists())
        with self.assertRaises(ValueError):
            shared.release_inputs(self.build, token, 'thesis')
        shared.release_inputs(self.build, token, 'quality')
        maintenance.maintain(self.build, **args)
        self.assertFalse(source.exists())

    def test_missing_input_never_creates_a_lease(self):
        with self.assertRaises(ValueError):
            shared.acquire_inputs(self.build, self.root, ['absent'], 'quality')
        with maintenance.database(self.build) as db:
            self.assertEqual(maintenance.rows(db), {})

    def test_historical_input_is_registered_without_changing_its_lifecycle(self):
        historical = self.build/'runs'/'historical'
        historical.mkdir()
        (historical/'.finished').touch()
        token = shared.acquire_inputs(self.build, self.root, ['historical'], 'quality')
        with maintenance.database(self.build) as db:
            records = maintenance.rows(db)
        self.assertIn('historical', maintenance.protected_names(records))
        self.assertNotIn('historical', records)
        shared.release_inputs(self.build, token, 'quality')
        self.assertTrue((historical/'.finished').exists())

    def test_external_historical_model_survives_asset_cleanup(self):
        from test_artifact_asset_cleanup import AssetCopiesTests
        from artifact_asset_cleanup import clean
        fixture = self.root/'asset-fixture'
        fixture.mkdir()
        run, source, target, catalog, lock = AssetCopiesTests().fixture(fixture)
        token = shared.acquire_inputs(fixture/'build', self.root, ['history'], 'quality')
        result = clean(fixture, catalog, lock, apply=True, busy=[], tracked=set())
        self.assertTrue(target.exists())
        self.assertEqual(result['items'], [])
        shared.release_inputs(fixture/'build', token, 'quality')
        clean(fixture, catalog, lock, apply=True, busy=[], tracked=set())
        self.assertFalse(target.exists())

    def test_external_historical_run_survives_directory_cleanup(self):
        import artifact_cleanup as cleanup
        run = self.build/'runs'/'historical'
        run.mkdir()
        (run/'.finished').touch()
        old = time.time()-8*86400
        os.utime(run/'.finished', (old, old))
        shared.acquire_inputs(self.build, self.root, ['historical'], 'quality')
        with patch.object(cleanup, 'BUILD', self.build), patch.object(maintenance, 'tracked_paths', return_value=set()), patch.object(sys, 'argv', ['cleanup', '--apply']):
            with self.assertRaises(SystemExit):
                cleanup.main()
        self.assertTrue(run.exists())


if __name__ == '__main__':
    unittest.main()
