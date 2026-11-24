"""Layered reads must honor a full patch catalog and preserve base chunks."""
import hashlib
from contextlib import closing
import json
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace as NS
import sqlite3
import unittest

try:
    from endfield_native.vfs_layers import LayeredVfs
except ImportError:
    LayeredVfs=None


class PlainCatalogBackend:
    DEFAULT_VFS_KEY_HEX=''

    @staticmethod
    def decrypt_blc(raw,key):return raw

    @staticmethod
    def parse_blc(raw,**expected):
        d=json.loads(raw);files=[NS(**x) for x in d['files']]
        return NS(group_hash=d['group'],version=3,chunks=[NS(**x) for x in d['chunks']],
                  iter_files=lambda:iter(files))

    @staticmethod
    def copy_virtual_file(source,target,item,version,key):
        source.seek(item.offset);data=source.read(item.length);target.write(data)
        return hashlib.md5(data).hexdigest().upper()


class LayeredVfsTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(callable(LayeredVfs),'Layered VFS reader is required')
        self.temp=TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.base=Path(self.temp.name)/'base';self.patch=Path(self.temp.name)/'patch'
        for root in [self.base,self.patch]:(root/'C3442D43').mkdir(parents=True)
        self.files={}
        self.old=self.entry('scene.bytes','A'*32,b'old')
        self.shared=self.entry('common.bytes','B'*32,b'common')
        self.new=self.entry('scene.bytes','C'*32,b'new')
        self.write_catalog(self.base,[self.old,self.shared])
        self.write_catalog(self.patch,[self.new,self.shared])
        for root,name,data in [(self.base,'A'*32,b'old'),(self.base,'B'*32,b'common'),(self.patch,'C'*32,b'new')]:
            (root/'C3442D43'/(name+'.chk')).write_bytes(data)
        self.view=LayeredVfs([self.base,self.patch],PlainCatalogBackend)

    def entry(self,name,chunk,data):
        return dict(file_name=name,file_chunk_md5_name=chunk,file_data_md5=hashlib.md5(data).hexdigest(),offset=0,length=len(data))

    def write_catalog(self,root,files):
        chunks={x['file_chunk_md5_name']:x['length'] for x in files}
        doc=dict(group='C3442D43',files=files,chunks=[dict(md5_name=k,length=v) for k,v in chunks.items()])
        (root/'C3442D43/C3442D43.blc').write_text(json.dumps(doc))

    def test_patch_catalog_selects_new_data_and_resolves_shared_base_chunk(self):
        self.assertEqual(self.view.read('c3442d43','SCENE.bytes'),b'new')
        self.assertEqual(self.view.read('C3442D43','common.bytes'),b'common')
        receipt=self.view.receipt()
        self.assertEqual(receipt['reads'][0]['chunk_layer'],1)
        self.assertEqual(receipt['reads'][1]['chunk_layer'],0)
        self.assertEqual(receipt['catalogs'][0]['catalog_layer'],1)

    def test_removed_virtual_file_does_not_fall_back_to_old_catalog(self):
        self.write_catalog(self.patch,[self.shared])
        with self.assertRaisesRegex(ValueError,'absent'):
            self.view.read('C3442D43','scene.bytes')

    def test_missing_updated_chunk_does_not_load_old_virtual_file(self):
        (self.patch/'C3442D43'/('C'*32+'.chk')).unlink()
        with self.assertRaisesRegex(ValueError,'chunk'):
            self.view.read('C3442D43','scene.bytes')

    def test_corrupt_payload_and_mid_snapshot_catalog_change_are_rejected(self):
        (self.patch/'C3442D43'/('C'*32+'.chk')).write_bytes(b'bad')
        with self.assertRaisesRegex(ValueError,'checksum'):
            self.view.read('C3442D43','scene.bytes')
        self.write_catalog(self.patch,[self.shared])
        with self.assertRaisesRegex(ValueError,'changed'):
            self.view.read('C3442D43','common.bytes')

    def test_new_higher_priority_catalog_invalidates_a_started_snapshot(self):
        (self.patch/'C3442D43/C3442D43.blc').unlink()
        self.assertEqual(self.view.read('C3442D43','scene.bytes'),b'old')
        self.write_catalog(self.patch,[self.new,self.shared])
        with self.assertRaisesRegex(ValueError,'changed'):
            self.view.read('C3442D43','scene.bytes')

    def test_copy_preserves_source_and_rejects_output_inside_layers(self):
        target=Path(self.temp.name)/'out.bin'
        self.view.copy('C3442D43','scene.bytes',target)
        self.assertEqual(target.read_bytes(),b'new')
        self.assertEqual((self.base/'C3442D43'/('A'*32+'.chk')).read_bytes(),b'old')
        with self.assertRaises(ValueError):
            self.view.copy('C3442D43','scene.bytes',self.patch/'out.bin')
        with self.assertRaises(FileExistsError):self.view.copy('C3442D43','scene.bytes',target)

    def test_duplicate_roots_and_invalid_group_are_rejected(self):
        with self.assertRaises(ValueError):LayeredVfs([self.base,self.base],PlainCatalogBackend)
        with self.assertRaises(ValueError):self.view.read('../bad','scene.bytes')

    def test_upstream_bridge_and_bundle_export_use_the_same_patch_view(self):
        def unsupported(*args,**kwargs):raise AssertionError('base reader must not consume patched requests')
        api=NS(load_group=unsupported,read_virtual_file=unsupported,extract_candidate_bundles=unsupported)
        self.view.install(api)
        self.assertEqual(api.read_virtual_file(self.base,'C3442D43','scene.bytes'),b'new')
        database=Path(self.temp.name)/'resources.sqlite'
        with closing(sqlite3.connect(database)) as connection:
            connection.execute('CREATE TABLE bundles(logical_name TEXT,group_hash TEXT,virtual_path TEXT)')
            connection.execute('INSERT INTO bundles VALUES (?,?,?)',('main/test.ab','C3442D43','scene.bytes'))
            connection.commit()
        output=Path(self.temp.name)/'export'
        result=api.extract_candidate_bundles(database,self.base,['main/test.ab'],output)
        self.assertEqual((output/'main/test.ab').read_bytes(),b'new')
        self.assertEqual(result['extracted_count'],'1')


if __name__=='__main__':unittest.main()
