import tempfile
import time
import unittest
from pathlib import Path
from artifact_cleanup import eligible, remove_artifact, inspect_tree

class CleanupTests(unittest.TestCase):
    def test_only_finished_expired_runs(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "run"
            p.mkdir()
            self.assertFalse(eligible(p, 14, time.time()))
            (p / ".finished").touch()
            self.assertFalse(eligible(p, 14, time.time()))
            self.assertTrue(eligible(p, 14, time.time() + 15 * 86400))
            (p / ".keep").touch()
            self.assertFalse(eligible(p, 14, time.time() + 15 * 86400))

    def test_boundary_and_tracked_protection(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            build = root / "build"
            build.mkdir()
            run = build / "run"
            run.mkdir()
            f = run / "evidence.txt"
            f.write_text("evidence")
            with self.assertRaises(ValueError):
                remove_artifact(build, run, {f.resolve()}, apply=True)
            with self.assertRaises(ValueError):
                remove_artifact(build, root, set(), apply=True)
            self.assertTrue(f.exists())
            remove_artifact(build, run, set(), apply=False)
            self.assertTrue(f.exists())
            remove_artifact(build, run, set(), apply=True)
            self.assertFalse(run.exists())

    def test_default_cli_dry_run_and_apply(self):
        import os
        from unittest.mock import patch
        import artifact_cleanup as cleanup
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            build = root / "build"
            runs = build / "runs"
            old = runs / "old"
            active = runs / "active"
            old.mkdir(parents=True)
            active.mkdir()
            marker = old / ".finished"
            marker.touch()
            expired = time.time() - 30 * 86400
            os.utime(marker, (expired, expired))
            os.utime(old, (expired, expired))
            with patch.object(cleanup, "ENGINE", root), patch.object(cleanup, "BUILD", build):
                for apply in (False, True):
                    with patch("sys.argv", ["cleanup"] + (["--apply"] if apply else [])), patch.object(cleanup.subprocess, "check_output", side_effect=[str(root), b""]):
                        cleanup.main()
                    self.assertEqual(old.exists(), not apply)
                    self.assertTrue(active.exists())

    def test_readonly_generated_file(self):
        import os
        import stat
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            run = root / "run"
            run.mkdir()
            file = run / "generated.pack"
            file.write_text("generated")
            os.chmod(file, stat.S_IREAD)
            try:
                remove_artifact(root, run, set(), apply=True)
                self.assertFalse(run.exists())
            finally:
                if file.exists():
                    os.chmod(file, stat.S_IWRITE)

    def test_tree_modification_time_delays_cleanup(self):
        import os
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            old = time.time() - 30 * 86400
            (root / ".finished").touch()
            os.utime(root / ".finished", (old, old))
            (root / "resumed.log").write_text("new output")
            size, newest = inspect_tree(root)
            self.assertGreater(size, 0)
            self.assertLess(time.time() - newest, 60)

    def test_symlink_protection(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            run = root / "run"
            run.mkdir()
            try:
                (run / "link").symlink_to(root, target_is_directory=True)
            except OSError:
                import _winapi
                _winapi.CreateJunction(str(root), str(run / "link"))
            with self.assertRaises(ValueError):
                remove_artifact(root, run, set(), apply=True)

if __name__ == "__main__":
    unittest.main()
