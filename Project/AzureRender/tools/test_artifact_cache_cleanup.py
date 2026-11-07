import hashlib,json,os,struct,tempfile,time,unittest
from pathlib import Path
from artifact_cache_cleanup import clean

class CacheTests(unittest.TestCase):
    def test_catalog_parent_traversal_is_rejected(self):
        from artifact_maintenance import real_path
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);outside=root/'outside';outside.mkdir()
            with self.assertRaises(ValueError):real_path(root/'build/runs/legacy/../../../outside')
            self.assertTrue(outside.exists())

    def test_daily_budget_cannot_be_reset_by_another_invocation(self):
        from artifact_maintenance import write_json
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'build').mkdir()
            write_json(root/'build/maintenance/cache-cleanup-daily-budget.json',dict(date=time.strftime('%Y-%m-%d'),reservedBytes=10*2**30))
            result=clean(root,root/'not-needed.json',root/'not-needed-lock.json',apply=True,busy=[],tracked=set())
            self.assertEqual(result['status'],'daily-budget-used');self.assertFalse(result['items'])

    def test_only_identical_old_generated_copies_are_removed(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);run=root/'build/runs/legacy';run.mkdir(parents=True)
            current=root/'current';current.mkdir();project=current/'project.azureproject';project.write_text('{}')
            source=current/'model.glb';source.write_bytes(b'canonical payload')
            cache=run/'old-project/.azure/cache/asset/42.azcache';cache.parent.mkdir(parents=True)
            cache.write_bytes(b'AZCACHE1'+struct.pack('<QQ',42,source.stat().st_size)+source.read_bytes())
            when=time.time()-3*86400;os.utime(cache,(when,when));(run/'.keep').touch()
            unknown=cache.with_name('43.azcache');unknown.write_bytes(b'AZCACHE1'+struct.pack('<QQ',43,3)+b'old');os.utime(unknown,(when,when))
            catalog=root/'catalog.json';catalog.write_text(json.dumps(dict(root=str(run),items=[dict(path=str(p),bytes=p.stat().st_size) for p in [cache,unknown]])))
            lock=root/'lock.json';lock.write_text(json.dumps(dict(project={str(project):hashlib.sha256(project.read_bytes()).hexdigest(),str(source):hashlib.sha256(source.read_bytes()).hexdigest()})))
            result=clean(root,catalog,lock,apply=True,busy=[],tracked=set())
            self.assertFalse(cache.exists());self.assertTrue(unknown.exists());self.assertEqual(source.read_bytes(),b'canonical payload')
            self.assertEqual(result['items'][0]['canonicalSource'],str(source));self.assertFalse(result['wholeRunBackupCreated'])

    def test_current_project_cache_and_tracked_content_are_kept(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);run=root/'build/runs/legacy';run.mkdir(parents=True);project=run/'project.azureproject';project.write_text('{}')
            source=run/'source.glb';source.write_bytes(b'payload');cache=run/'.azure/cache/id/1.azcache';cache.parent.mkdir(parents=True);cache.write_bytes(b'AZCACHE1'+struct.pack('<QQ',1,7)+b'payload')
            when=time.time()-3*86400;os.utime(cache,(when,when));catalog=root/'catalog.json';catalog.write_text(json.dumps(dict(root=str(run),items=[dict(path=str(cache),bytes=cache.stat().st_size)])))
            lock=root/'lock.json';lock.write_text(json.dumps(dict(project={str(project):hashlib.sha256(project.read_bytes()).hexdigest(),str(source):hashlib.sha256(source.read_bytes()).hexdigest()})))
            result=clean(root,catalog,lock,apply=True,busy=[],tracked={cache.resolve()});self.assertTrue(cache.exists());self.assertFalse(result['items'])

if __name__=='__main__':unittest.main()
