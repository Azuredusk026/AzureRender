import hashlib,json,os,tempfile,time,unittest
from pathlib import Path
from unittest.mock import patch
from artifact_asset_cleanup import clean,create_catalog

class AssetCopiesTests(unittest.TestCase):
    def fixture(self,root):
        run=root/'build/runs/history';run.mkdir(parents=True);(run/'.finished').touch();(run/'.keep').touch()
        current=root/'current';current.mkdir();project=current/'scene.azureproject';project.write_text('{}')
        source=current/'hero.glb';source.write_bytes(b'exact model payload')
        target=run/'stage/assets/hero.glb';target.parent.mkdir(parents=True);target.write_bytes(source.read_bytes())
        when=time.time()-3*86400;os.utime(target,(when,when))
        catalog=root/'build/maintenance/assets.json';catalog.parent.mkdir(parents=True);catalog.write_text(json.dumps(dict(schemaVersion=1,kind='generated-model-copies',root=str(run),items=[dict(path=str(target),bytes=target.stat().st_size)])))
        lock=root/'lock.json';lock.write_text(json.dumps(dict(project={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [project,source]})))
        return run,source,target,catalog,lock

    def test_verified_duplicate_is_removed_with_surviving_source_receipt(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);run,source,target,catalog,lock=self.fixture(root)
            result=clean(root,catalog,lock,apply=True,busy=[],tracked=set())
            self.assertFalse(target.exists());self.assertEqual(source.read_bytes(),b'exact model payload');self.assertTrue((run/'.keep').exists())
            self.assertTrue(result['items'][0]['applied']);self.assertEqual(result['items'][0]['canonicalSource'],str(source));self.assertFalse(result['externalStorageUsed'])
            journal=json.loads((root/'build/maintenance/asset-copy-cleanup-current-batch.json').read_text());self.assertEqual(journal['items'][0]['path'],str(target))

    def test_current_input_and_tracked_copy_are_kept(self):
        for protected in ['input','tracked']:
            with self.subTest(protected=protected),tempfile.TemporaryDirectory() as d:
                root=Path(d);_,source,target,catalog,lock=self.fixture(root)
                if protected=='input':
                    value=json.loads(lock.read_text());value['project'][str(target)]=hashlib.sha256(target.read_bytes()).hexdigest();lock.write_text(json.dumps(value))
                result=clean(root,catalog,lock,apply=True,busy=[],tracked={target.resolve()} if protected=='tracked' else set())
                self.assertTrue(target.exists());self.assertFalse(result['items'])

    def test_changed_model_and_nested_git_are_kept(self):
        for protected in ['different','git']:
            with self.subTest(protected=protected),tempfile.TemporaryDirectory() as d:
                root=Path(d);run,source,target,catalog,lock=self.fixture(root)
                if protected=='different':target.write_bytes(b'other model payload');stamp=time.time()-3*86400;os.utime(target,(stamp,stamp))
                else:(run/'stage/.git').mkdir()
                result=clean(root,catalog,lock,apply=True,busy=[],tracked=set())
                self.assertTrue(target.exists());self.assertFalse(result['items'])

    def test_day_byte_and_file_limits_persist_across_invocations(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);run,source,target,catalog,lock=self.fixture(root)
            result=clean(root,catalog,lock,apply=True,limit_bytes=len(source.read_bytes()),maximum_files=1,busy=[],tracked=set());self.assertFalse(target.exists())
            target.write_bytes(source.read_bytes());stamp=time.time()-3*86400;os.utime(target,(stamp,stamp))
            again=clean(root,catalog,lock,apply=True,limit_bytes=len(source.read_bytes()),maximum_files=1,busy=[],tracked=set());self.assertTrue(target.exists());self.assertEqual(again['status'],'daily-budget-used')

    def test_busy_work_and_unusable_budget_skip_payload_reads(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);_,source,target,catalog,lock=self.fixture(root)
            with patch('artifact_asset_cleanup.file_digest',side_effect=AssertionError('Skipped work read payload')):
                result=clean(root,catalog,lock,apply=True,limit_bytes=1,busy=[],tracked=set());self.assertFalse(result['items'])
                result=clean(root,catalog,lock,apply=True,busy=['active-renderer'],tracked=set());self.assertEqual(result['status'],'deferred-active-work')
            self.assertTrue(target.exists())

    def test_preview_and_outside_catalog_preserve_files(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);_,source,target,catalog,lock=self.fixture(root)
            result=clean(root,catalog,lock,apply=False,busy=[],tracked=set());self.assertTrue(target.exists());self.assertEqual(len(result['items']),1);self.assertFalse(result['items'][0]['applied'])
            value=json.loads(catalog.read_text());value['items'][0]['path']=str(source);catalog.write_text(json.dumps(value))
            with self.assertRaises(ValueError):clean(root,catalog,lock,apply=True,busy=[],tracked=set())
            self.assertTrue(source.exists());self.assertTrue(target.exists())

    def test_catalog_reuses_declared_history_and_prunes_generated_cache(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);run,source,target,catalog,lock=self.fixture(root)
            cache=run/'stage/.azure/cache/hero.glb';cache.parent.mkdir(parents=True);cache.write_bytes(source.read_bytes())
            result=create_catalog(root,run,lock,catalog,busy=[])
            self.assertEqual(result['files'],1);self.assertEqual(json.loads(catalog.read_text())['items'][0]['path'],str(target))
            self.assertTrue(target.exists());self.assertTrue(cache.exists())

    def test_invalid_budgets_are_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);_,source,target,catalog,lock=self.fixture(root)
            for values in [dict(seconds=float('nan')),dict(seconds=0),dict(limit_bytes=-1),dict(maximum_files=0)]:
                with self.subTest(values=values),self.assertRaises(ValueError):clean(root,catalog,lock,busy=[],tracked=set(),**values)
            self.assertTrue(target.exists())

    def test_changed_input_lock_aborts_before_removal(self):
        from artifact_asset_cleanup import file_digest
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);_,source,target,catalog,lock=self.fixture(root)
            def changing(path,deadline):
                result=file_digest(path,deadline)
                if path==source:lock.write_text(lock.read_text()+'\n')
                return result
            with patch('artifact_asset_cleanup.file_digest',side_effect=changing),self.assertRaises(ValueError):clean(root,catalog,lock,apply=True,busy=[],tracked=set())
            self.assertTrue(target.exists());self.assertTrue(source.exists())

    def test_shared_file_identity_is_hashed_once_and_canonical_name_survives(self):
        from artifact_asset_cleanup import file_digest
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);run,source,target,catalog,lock=self.fixture(root)
            target.unlink();os.link(source,target)
            second=run/'another/assets/hero.glb';second.parent.mkdir(parents=True);os.link(source,second)
            stamp=time.time()-3*86400;os.utime(source,(stamp,stamp))
            value=json.loads(catalog.read_text());value['items'].append(dict(path=str(second),bytes=source.stat().st_size));catalog.write_text(json.dumps(value))
            with patch('artifact_asset_cleanup.file_digest',wraps=file_digest) as hashed:
                result=clean(root,catalog,lock,apply=True,busy=[],tracked=set())
            self.assertFalse(target.exists());self.assertFalse(second.exists());self.assertEqual(source.read_bytes(),b'exact model payload')
            self.assertEqual(hashed.call_count,1)
            self.assertEqual(result['payloadFilesHashed'],1);self.assertEqual(result['reusedFileIdentityProofs'],2)

    def test_exhausted_remaining_bytes_skip_input_enumeration(self):
        from artifact_asset_cleanup import MAXIMUM_BYTES
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);_,source,target,catalog,lock=self.fixture(root)
            (root/'build/maintenance/asset-copy-cleanup-daily-budget.json').write_text(json.dumps(dict(date=time.strftime('%Y-%m-%d'),reservedBytes=MAXIMUM_BYTES-1,reservedFiles=0)))
            with patch('artifact_asset_cleanup.frozen_sources',side_effect=AssertionError('Unusable daily quota enumerated inputs')):
                result=clean(root,catalog,lock,apply=True,busy=[],tracked=set())
            self.assertEqual(result['status'],'daily-budget-used');self.assertTrue(target.exists())

if __name__=='__main__':unittest.main()
