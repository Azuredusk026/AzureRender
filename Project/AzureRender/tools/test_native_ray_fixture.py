"""Validate immutable replay payloads using real files and independent hashes."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from native_render.ray_vulkan_shader import reference_source

try:
    from native_render.ray_fixture import verify_payloads
except ImportError:
    verify_payloads=None


class FixtureContracts(unittest.TestCase):
    def setUp(self):
        self.assertTrue(callable(verify_payloads),'Replay payload verifier API missing')
        self.directory=tempfile.TemporaryDirectory(prefix='azure-ray-payload-')
        self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name)
        data=b'\x01\x02\x03\x04'
        (self.root/'input.bin').write_bytes(data)
        self.row=dict(file='input.bin',bytes=4,sha256=hashlib.sha256(data).hexdigest())
        self.manifest=dict(schemaVersion=1,status='complete',constants=[self.row],
            tiles=self.row,textures=[],outputs=[])

    def test_exact_payloads_are_accepted(self):
        self.assertEqual(verify_payloads(self.root,self.manifest),8)

    def test_manifest_must_be_an_object(self):
        for value in (None,[],True):
            with self.assertRaises(ValueError):verify_payloads(self.root,value)

    def test_single_byte_change_is_rejected(self):
        (self.root/'input.bin').write_bytes(b'\x01\x02\x03\x05')
        with self.assertRaises(ValueError):verify_payloads(self.root,self.manifest)

    def test_wrong_length_is_rejected(self):
        (self.root/'input.bin').write_bytes(b'12345')
        with self.assertRaises(ValueError):verify_payloads(self.root,self.manifest)

    def test_path_escape_is_rejected(self):
        self.row['file']='../input.bin'
        with self.assertRaises(ValueError):verify_payloads(self.root,self.manifest)

    def test_unsupported_or_incomplete_manifest_is_rejected(self):
        for value in (2,True,1.0):
            self.manifest['schemaVersion']=value
            with self.assertRaises(ValueError):verify_payloads(self.root,self.manifest)
        self.manifest['schemaVersion']=1;self.manifest['status']='partial'
        with self.assertRaises(ValueError):verify_payloads(self.root,self.manifest)

    def test_byte_budget_and_integer_contract_are_enforced(self):
        with self.assertRaises(ValueError):verify_payloads(self.root,self.manifest,max_bytes=7)
        for value in (0,-1,True,4.0,128*1024*1024+1):
            self.row['bytes']=value
            with self.assertRaises(ValueError):verify_payloads(self.root,self.manifest)

    def test_storage_initial_and_reference_are_checked(self):
        self.manifest['outputs']=[dict(initial=self.row,reference={**self.row,'sha256':'0'*64})]
        with self.assertRaises(ValueError):verify_payloads(self.root,self.manifest)

    def test_invalid_digest_and_missing_file_are_rejected(self):
        self.row['sha256']='invalid'
        with self.assertRaises(ValueError):verify_payloads(self.root,self.manifest)
        self.row['sha256']='0'*64;(self.root/'input.bin').unlink()
        with self.assertRaises(ValueError):verify_payloads(self.root,self.manifest)


class ShaderProvenance(unittest.TestCase):
    def test_reference_source_and_binary_match_receipt(self):
        engine=Path(__file__).resolve().parents[1]
        fixture=engine/'tests/fixtures/native_ray_vulkan'
        receipt=json.loads((fixture/'source.json').read_text(encoding='utf-8'))
        source=(engine/receipt['source']).read_bytes()
        generated=reference_source(source).encode('utf-8')
        self.assertEqual(hashlib.sha256(generated).hexdigest(),receipt['files']['reference.hlsl']['sha256'])
        binary=(fixture/'reference.spv').read_bytes()
        self.assertEqual(hashlib.sha256(binary).hexdigest(),receipt['files']['reference.spv']['sha256'])

    def test_changed_native_source_is_rejected(self):
        with self.assertRaises(ValueError):reference_source(b'changed source')


if __name__=='__main__':unittest.main()
