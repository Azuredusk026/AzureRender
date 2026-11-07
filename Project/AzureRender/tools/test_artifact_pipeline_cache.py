import os
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import patch
import artifact_pipeline_cache as cache
import artifact_maintenance as maintenance


class PipelineCacheCleanupTests(unittest.TestCase):
    def setUp(self):
        base=Path(__file__).resolve().parents[1]/'build/maintenance/test-tmp'
        base.mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=base)
        self.build=Path(self.temp.name)/'build';self.root=self.build/'cache/vulkan';self.root.mkdir(parents=True)

    def tearDown(self):self.temp.cleanup()

    def entry(self,driver,age=0,size=20):
        path=self.root/f'vk1-10de-28a0-{driver:x}-000102030405060708090a0b0c0d0e0f.azpc'
        path.write_bytes(b'x'*size);stamp=time.time()-age;os.utime(path,(stamp,stamp));return path

    def test_expired_cache_only(self):
        old=self.entry(1,8*86400);fresh=self.entry(2);foreign=self.root/'vk1-authored.azpc';foreign.write_text('source')
        result=cache.clean(self.build,apply=True,tracked=set())
        self.assertEqual(result['files'],1);self.assertFalse(old.exists());self.assertTrue(fresh.exists());self.assertTrue(foreign.exists())

    def test_pool_cap_and_tracked_content(self):
        first=self.entry(1,60);second=self.entry(2,30);third=self.entry(3)
        result=cache.clean(self.build,apply=True,tracked={first},maximum_pool=40)
        self.assertTrue(first.exists());self.assertFalse(second.exists());self.assertTrue(third.exists());self.assertEqual(result['files'],1)

    def test_preview_has_no_deletion(self):
        old=self.entry(1,8*86400)
        result=cache.clean(self.build,apply=False,tracked=set())
        self.assertEqual(result['files'],1);self.assertTrue(old.exists())

    def test_writer_lock_defers_cleanup(self):
        old=self.entry(1,8*86400)
        with cache.pool_lock(self.root) as locked:
            self.assertTrue(locked);result=cache.clean(self.build,apply=True,tracked=set())
        self.assertEqual(result['status'],'busy');self.assertTrue(old.exists())

    def test_bounded_directory_inspection(self):
        old=self.entry(1,8*86400)
        for i in range(65):(self.root/f'authored-{i}.txt').write_text('source')
        result=cache.clean(self.build,apply=True,tracked=set())
        self.assertEqual(result['status'],'entry-limit');self.assertTrue(old.exists())

    def test_nested_cache_repository_is_protected(self):
        (self.root.parent/'.git').write_text('gitdir: fixture')
        old=self.entry(1,8*86400)
        result=cache.clean(self.build,apply=True,tracked=set())
        self.assertEqual(result['status'],'protected-repository');self.assertTrue(old.exists())

    def test_active_work_defers_daily_without_expired_tasks(self):
        with patch.object(maintenance,'active_processes',return_value=[dict(pid=777,name='AzurePlayer.exe')]),patch.object(cache,'clean',return_value={}) as clean:
            result=maintenance.daily(self.build,apply=True)
        self.assertEqual(result['managed']['status'],'deferred-active-work');clean.assert_not_called()


if __name__=='__main__':unittest.main()
