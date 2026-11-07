"""Exercise recovery bytes and destructive boundaries in isolated fixtures."""
import json
import os
from pathlib import Path
import tempfile
import time
import unittest
import sys
import subprocess
from unittest.mock import patch

import artifact_maintenance as maintenance


class MaintenanceTests(unittest.TestCase):
    def test_git_tracking_protection_uses_index_paths_with_one_root_resolution(self):
        subprocess.run(['git','init','--quiet',str(self.root)],check=True,capture_output=True)
        folder=self.root/'sources';folder.mkdir()
        expected=set()
        for i in range(100):
            path=folder/(str(i)+'.bin');path.write_bytes(b'source');expected.add(path.absolute())
        subprocess.run(['git','-C',str(self.root),'add','sources'],check=True,capture_output=True)
        resolve=Path.resolve;resolved=[]
        def observed(path,*args,**kwargs):resolved.append(path);return resolve(path,*args,**kwargs)
        with patch.object(Path,'resolve',observed):actual=maintenance.tracked_paths(self.root)
        self.assertEqual(actual,expected);self.assertLessEqual(len(resolved),1)

    def test_indexed_symbolic_link_protects_name_and_referent(self):
        subprocess.run(['git','init','--quiet',str(self.root)],check=True,capture_output=True)
        source=self.root/'source.glb';source.write_bytes(b'canonical')
        link=self.root/'indexed.glb'
        try:link.symlink_to(source.name)
        except OSError:self.skipTest('Symbolic link creation is unavailable')
        subprocess.run(['git','-C',str(self.root),'-c','core.symlinks=true','add','indexed.glb'],check=True,capture_output=True)
        actual=maintenance.tracked_paths(self.root)
        self.assertIn(link.absolute(),actual);self.assertIn(source.absolute(),actual)

    def test_daily_asset_copy_policy_removes_only_verified_model(self):
        from test_artifact_asset_cleanup import AssetCopiesTests
        _,source,target,catalog,lock=AssetCopiesTests().fixture(self.root)
        maintenance.write_json(self.build/'maintenance/asset-copy-cleanup-policy.json',dict(catalog=str(catalog),lock=str(lock),maximumBytes=source.stat().st_size,maximumFiles=1))
        with patch.object(maintenance,'active_processes',return_value=[]),patch('artifact_asset_cleanup.active_processes',return_value=[]):
            result=maintenance.daily(self.build,apply=True)
        self.assertFalse(target.exists());self.assertTrue(source.exists());self.assertEqual(result['assetCopies']['files'],1)

    def test_daily_active_work_defers_model_cleanup_without_index(self):
        from test_artifact_asset_cleanup import AssetCopiesTests
        _,source,target,catalog,lock=AssetCopiesTests().fixture(self.root)
        maintenance.write_json(self.build/'maintenance/asset-copy-cleanup-policy.json',dict(catalog=str(catalog),lock=str(lock)))
        with patch.object(maintenance,'active_processes',return_value=['active-player']):result=maintenance.daily(self.build,apply=True)
        self.assertTrue(target.exists());self.assertEqual(result['managed']['status'],'deferred-active-work');self.assertIsNone(result['assetCopies'])

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.build = self.root/'build'
        (self.build/'runs').mkdir(parents=True)
        self.cold = self.root/'cold'

    def tearDown(self):
        self.temp.cleanup()

    def create(self, name='example', payload=b'generated output',recovery=True):
        path = self.build/'runs'/name
        maintenance.create_run(self.build,path,dict(plannedBytes=64*2**20),retain_recovery=recovery)
        (path/'nested').mkdir()
        (path/'nested'/'payload.bin').write_bytes(payload)
        return path

    def close(self, path):
        maintenance.seal(self.build,path)
        (path/'.finished').touch()

    def apply(self, **kwargs):
        return maintenance.maintain(self.build,apply=True,cold=self.cold,
                                    now=time.time()+8*86400,busy=[],tracked=set(),git=lambda:'same',**kwargs)

    def test_status_has_no_recursive_scan_or_database_creation(self):
        path=self.build/'runs'/'legacy';path.mkdir();(path/'.keep').touch()
        with patch.object(maintenance,'inventory',side_effect=AssertionError('No tree scan')):
            result=maintenance.status(self.build)
        self.assertEqual(result['historicalTasks'],1)
        self.assertFalse(result['recursiveScan'])
        self.assertFalse((self.build/'maintenance').exists())

    def test_pool_cap_blocks_creation_before_directory_exists(self):
        self.create('first')
        with patch.dict(maintenance.POLICY,poolBytes=64*2**20):
            with self.assertRaises(ValueError):self.create('second')
        self.assertFalse((self.build/'runs'/'second').exists())

    def test_append_reservation_is_atomic_and_consumes_pool(self):
        path=self.create('first')
        other=self.create('second')
        with patch.dict(maintenance.POLICY,poolBytes=150*2**20):
            self.assertTrue(maintenance.reserve_write(self.build,path,0,80*2**20)['allowed'])
            self.assertFalse(maintenance.reserve_write(self.build,other,0,80*2**20)['allowed'])

    def test_expired_managed_recovery_has_finite_retention(self):
        path=self.create();self.close(path);self.apply()
        result=maintenance.maintain(self.build,apply=True,cold=self.cold,now=time.time()+100*86400,busy=[],tracked=set(),git=lambda:'same')
        self.assertEqual(result['items'][0]['expiredRecoveryTask'],path.name)
        self.assertFalse(list(self.cold.glob('*.zip')))

    def test_recovery_round_trip_and_preview_preservation(self):
        payload=bytes(range(256))*100
        path=self.create(payload=payload);self.close(path)
        preview=maintenance.maintain(self.build,now=time.time()+8*86400)
        self.assertEqual(preview['candidates'],[str(path)])
        self.assertTrue(path.exists())
        result=self.apply();self.assertEqual(result['status'],'completed')
        self.assertFalse(path.exists())
        maintenance.restore(self.build,path.name)
        self.assertEqual((path/'nested/payload.bin').read_bytes(),payload)
        self.assertFalse((path/'.finished').exists())

    def test_keep_and_active_input_lease_prevent_removal(self):
        path=self.create();self.close(path);(path/'.keep').touch()
        self.assertFalse(self.apply()['items'])
        (path/'.keep').unlink()
        other=self.build/'runs'/'consumer'
        maintenance.create_run(self.build,other,dict(plannedBytes=64*2**20),[path.name])
        self.assertFalse(self.apply()['items'])
        self.close(other)
        self.assertFalse(self.apply()['status']=='blocked')
        self.assertFalse(path.exists())

    def test_busy_process_defers_without_archiving(self):
        path=self.create();self.close(path)
        result=maintenance.maintain(self.build,apply=True,cold=self.cold,now=time.time()+8*86400,busy=['build'],tracked=set(),git=lambda:'same')
        self.assertEqual(result['status'],'deferred-active-work')
        self.assertFalse(self.cold.exists());self.assertTrue(path.exists())

    def test_modified_sealed_content_is_preserved_and_quarantined(self):
        path=self.create();self.close(path);(path/'nested/payload.bin').write_bytes(b'new content')
        result=self.apply();self.assertEqual(result['status'],'blocked');self.assertTrue(path.exists())
        self.assertFalse(self.apply()['candidates'])

    def test_tracked_file_and_nested_worktree_are_protected(self):
        path=self.create();self.close(path)
        result=maintenance.maintain(self.build,apply=True,cold=self.cold,now=time.time()+8*86400,busy=[],tracked={(path/'nested/payload.bin').resolve()},git=lambda:'same')
        self.assertEqual(result['status'],'blocked');self.assertTrue(path.exists())
        other=self.create('worktree');(other/'.git').write_text('gitdir: elsewhere')
        with self.assertRaises(ValueError):self.close(other)

    def test_archive_corruption_blocks_restore(self):
        path=self.create();self.close(path);self.apply()
        archive=next(self.cold.glob('*.zip'));archive.write_bytes(b'broken')
        with self.assertRaises(ValueError):maintenance.restore(self.build,path.name)
        self.assertFalse(path.exists())

    def test_resume_during_archive_is_not_overwritten_or_deleted(self):
        path=self.create();self.close(path);original=maintenance.verify_archive;fired=[]
        def verify(archive,*args,**kwargs):
            result=original(archive,*args,**kwargs)
            if Path(archive).suffix=='.zip' and not fired:
                fired.append(True);maintenance.resume(self.build,path.name)
            return result
        with patch.object(maintenance,'verify_archive',side_effect=verify):self.apply()
        self.assertTrue(path.exists())
        with maintenance.database(self.build) as db:record=maintenance.rows(db)[path.name]
        self.assertEqual(record['status'],'active');self.assertIsNone(record['blocked'])

    def test_resealed_generation_is_not_deleted_by_old_candidate(self):
        path=self.create();self.close(path);original=maintenance.verify_archive;fired=[]
        def verify(archive,*args,**kwargs):
            result=original(archive,*args,**kwargs)
            if Path(archive).suffix=='.zip' and not fired:
                fired.append(True);maintenance.resume(self.build,path.name);self.close(path)
            return result
        with patch.object(maintenance,'verify_archive',side_effect=verify):self.apply()
        self.assertTrue(path.exists())
        with maintenance.database(self.build) as db:record=maintenance.rows(db)[path.name]
        self.assertEqual(record['status'],'closed');self.assertIsNone(record['blocked'])

    def test_restore_during_expiry_keeps_archive_and_reservation(self):
        path=self.create();self.close(path);self.apply();original=maintenance.digest;fired=[]
        def digest(archive):
            result=original(archive)
            if Path(archive).suffix=='.zip' and not fired:
                fired.append(True);maintenance.restore(self.build,path.name)
            return result
        with patch.object(maintenance,'digest',side_effect=digest):
            maintenance.maintain(self.build,apply=True,cold=self.cold,now=time.time()+100*86400,busy=[],tracked=set(),git=lambda:'same')
        self.assertTrue(path.exists());self.assertTrue(list(self.cold.glob('*.zip')))
        with maintenance.database(self.build) as db:record=maintenance.rows(db)[path.name]
        self.assertEqual(record['status'],'active');self.assertGreater(record['reservedBytes'],0)

    def test_partial_restoration_remains_counted_and_protected(self):
        path=self.create();self.close(path);self.apply()
        def partial(source,target,*args):target.write(b'partial');raise OSError('Injected storage failure')
        with patch.object(maintenance.shutil,'copyfileobj',side_effect=partial):
            with self.assertRaises(OSError):maintenance.restore(self.build,path.name)
        with maintenance.database(self.build) as db:record=maintenance.rows(db)[path.name]
        self.assertEqual(record['status'],'blocked-restore');self.assertGreater(record['reservedBytes'],0)
        self.assertTrue(path.exists());self.assertFalse(self.apply()['candidates'])

    def test_guard_stops_only_owned_over_budget_child(self):
        path=self.create()
        command=[sys.executable,'-c',"from pathlib import Path;import time;Path(r'"+str(path/'too-large.bin')+"').write_bytes(b'x'*2000000);time.sleep(10)"]
        with patch.dict(maintenance.POLICY,runBytes=1000000):
            result=maintenance.guarded_run(self.build,path.name,command,timeout=5)
        self.assertEqual(result['status'],'budget-stopped');self.assertLess(result['seconds'],8)
        self.assertTrue(path.exists())

    def test_guard_caps_log_without_stopping_successful_command(self):
        path=self.create()
        command=[sys.executable,'-c',"import sys;sys.stdout.buffer.write(b'x'*(20*1024*1024))"]
        result=maintenance.guarded_run(self.build,path.name,command,timeout=5)
        self.assertEqual(result['exitCode'],0);self.assertTrue(result['logTruncated'])
        self.assertLessEqual((path/'process.log').stat().st_size,16*2**20)

    def test_restore_rejects_path_traversal_without_writing(self):
        for name in ['../outside','/absolute','a/../b','C:/outside','nested\\file']:
            with self.assertRaises(ValueError):maintenance.relative_member(name)
        with self.assertRaises(ValueError):maintenance.restore(self.build,'../outside')
        self.assertFalse((self.root/'outside').exists())

    def test_batch_limit_and_resume_invalidate_eligibility(self):
        for index in range(3):self.close(self.create('task-'+str(index)))
        with patch.dict(maintenance.POLICY,maximumBatch=2):result=self.apply()
        self.assertEqual(len(result['items']),2)
        path=self.build/'runs'/'task-2';maintenance.resume(self.build,path.name)
        self.assertFalse((path/'.finished').exists());self.assertFalse(self.apply()['candidates'])

    def test_daily_rebuildable_cleanup_keeps_small_evidence_without_full_backup(self):
        path=self.create(payload=b'x'*200000,recovery=False)
        (path/'candidate.cpp').write_text('int main(){return 0;}')
        (path/'results.json').write_text('{"status":"isolated"}')
        self.close(path)
        result=maintenance.maintain(self.build,apply=True,cold=self.cold,now=time.time()+2*86400,busy=[],tracked=set(),git=lambda:'same')
        self.assertFalse(path.exists());self.assertFalse(self.cold.exists())
        self.assertIsNone(result['items'][0]['archive'])
        evidence=Path(result['items'][0]['evidence']);self.assertLess(evidence.stat().st_size,10000)
        import zipfile
        with zipfile.ZipFile(evidence) as package:
            self.assertEqual(package.read('files/candidate.cpp'),b'int main(){return 0;}')
            self.assertNotIn('files/nested/payload.bin',package.namelist())

    def test_compact_evidence_has_finite_retention(self):
        path=self.create(recovery=False);self.close(path);self.apply()
        result=maintenance.maintain(self.build,apply=True,cold=self.cold,now=time.time()+40*86400,busy=[],tracked=set(),git=lambda:'same')
        self.assertEqual(result['items'][0]['expiredEvidenceTask'],path.name)
        self.assertFalse(list((self.build/'maintenance/evidence').glob('*.zip')))

    def test_incomplete_existing_evidence_cannot_be_sealed_as_verified(self):
        path=self.create(recovery=False);(path/'recipe.ps1').write_text('Write-Output example');self.close(path)
        with maintenance.database(self.build) as db:record=maintenance.rows(db)[path.name]
        Path(record['evidence']).write_bytes(b'incomplete zip')
        maintenance.resume(self.build,path.name)
        import zipfile
        with self.assertRaises(zipfile.BadZipFile):self.close(path)
        self.assertTrue(path.exists());self.assertFalse((path/'.finished').exists())

    def test_resealing_does_not_leave_permanent_unreferenced_evidence(self):
        path=self.create(recovery=False);self.close(path)
        with maintenance.database(self.build) as db:old=Path(maintenance.rows(db)[path.name]['evidence'])
        maintenance.resume(self.build,path.name);(path/'recipe.py').write_text('print(1)');self.close(path)
        when=time.time()-40*86400;os.utime(old,(when,when))
        result=maintenance.maintain(self.build,apply=True,cold=self.cold,busy=[],tracked=set(),git=lambda:'same')
        self.assertFalse(old.exists());self.assertTrue(path.exists())
        self.assertTrue(any('expiredUnreferencedEvidence' in i for i in result['items']))

    @unittest.skipUnless(os.name=='nt','Windows process audit')
    def test_empty_process_audit_fails_closed(self):
        import subprocess
        with patch.object(maintenance.subprocess,'run',return_value=subprocess.CompletedProcess([],0,stdout='',stderr='')):
            with self.assertRaises(ValueError):maintenance.active_processes(self.root)

    def test_junction_or_symlink_is_rejected(self):
        path=self.create();outside=self.root/'outside';outside.mkdir()
        try:(path/'linked').symlink_to(outside,target_is_directory=True)
        except OSError:
            import _winapi
            _winapi.CreateJunction(str(outside),str(path/'linked'))
        with self.assertRaises(ValueError):self.close(path)
        self.assertTrue(outside.exists())


if __name__=='__main__':unittest.main()
