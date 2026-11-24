import tempfile
import time
import unittest
from pathlib import Path
from artifact_cleanup import eligible, remove_artifact, inspect_tree, TrackedPaths

class CleanupTests(unittest.TestCase):
    def test_prefix_index_matches_legacy_ancestor_protection(self):
        import random
        rng=random.Random(101)
        with tempfile.TemporaryDirectory() as d:
            root=Path(d).resolve()
            paths={root/('group'+str(rng.randrange(20)))/('nested'+str(rng.randrange(10)))/(str(i)+'.bin') for i in range(200)}
            tracked=TrackedPaths(paths)
            candidates=[root,root/'group1',root/'group10',root/'group100',*sorted(paths)[:20]]
            candidates.extend(root/('group'+str(i))/('nested'+str(j)) for i in range(25) for j in range(12))
            for candidate in candidates:
                expected=any(p==candidate or candidate in p.parents for p in paths)
                self.assertEqual(tracked.protects(candidate),expected,str(candidate))
            self.assertFalse(tracked.protects(root/'group1-sibling'))
            self.assertEqual(tracked,paths)

    def test_protected_directory_roots_and_case(self):
        import os
        with tempfile.TemporaryDirectory() as d:
            root=Path(d).resolve();module=root/'module';tracked=TrackedPaths({module},[module])
            self.assertTrue(tracked.protects(module/'nested'/'generated.bin'))
            self.assertFalse(tracked.protects(root/'module-copy'/'generated.bin'))
            self.assertTrue(tracked.protects(root));self.assertFalse(TrackedPaths().protects(root))
            self.assertTrue(TrackedPaths(protected_roots=[module]).protects(root))
            if os.name=='nt':self.assertTrue(tracked.protects(Path(str(module/'nested').upper())))

    def test_unicode_and_space_prefix_boundaries(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d).resolve();file=root/'资料 甲'/'数据.bin'
            tracked=TrackedPaths([file])
            self.assertTrue(tracked.protects(file));self.assertTrue(tracked.protects(file.parent))
            self.assertFalse(tracked.protects(root/'资料 甲-副本'))

    def test_indexed_tracking_guard_does_not_walk_all_paths(self):
        class Indexed(set):
            def __init__(self, protected):
                super().__init__();self.protected=protected;self.queries=[]
            def protects(self, path):
                self.queries.append(path);return self.protected
            def __iter__(self):
                raise AssertionError("Indexed protection must not scan tracked paths per artifact")
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);path=root/'generated.bin';path.write_bytes(b'owned')
            protected=Indexed(True)
            with self.assertRaises(ValueError):remove_artifact(root,path,protected,apply=True)
            self.assertTrue(path.exists());self.assertEqual(protected.queries,[path.resolve()])
            untracked=Indexed(False);remove_artifact(root,path,untracked,apply=True)
            self.assertFalse(path.exists());self.assertEqual(untracked.queries,[path.resolve()])

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
