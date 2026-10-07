"""Check installed source snapshots, ownership and integrity."""
import json
from pathlib import Path
import tempfile
import unittest
from write_engine_contracts import write_bundle, verify_bundle
class ContractBundleTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.source=self.root/'source';self.output=self.root/'contracts'
        (self.source/'src').mkdir(parents=True)
        (self.source/'src/Service.hpp').write_bytes(b'struct Service {};\r\n')
        (self.source/'schemas').mkdir();(self.source/'schemas/service.json').write_text('{"version":1}',encoding='utf-8')
        self.manifest={'schemaVersion':1,'referenceCommit':'fixture','items':[{'id':'A1','implementation':{
            'interfaces':['Service'],'interfaceFiles':['src/Service.hpp'],'verification':{'adoption':'adapted-adoption'},'scope':'Snapshot'}}]}
    def test_real_source_bytes_and_schemas_are_installed(self):
        write_bundle(self.source,self.manifest,self.output)
        self.assertEqual((self.output/'src/Service.hpp').read_bytes(),b'struct Service {};\r\n')
        self.assertTrue((self.output/'schemas/service.json').is_file())
        self.assertEqual(verify_bundle(self.output)['coveredItems'],1)
    def test_tampered_interface_is_rejected(self):
        write_bundle(self.source,self.manifest,self.output)
        (self.output/'src/Service.hpp').write_text('tampered',encoding='utf-8')
        with self.assertRaisesRegex(ValueError,'hash'):verify_bundle(self.output)
    def test_missing_input_preserves_existing_bundle(self):
        write_bundle(self.source,self.manifest,self.output);saved=(self.output/'manifest.json').read_bytes()
        self.manifest['items'][0]['implementation']['interfaceFiles']=['missing.hpp']
        with self.assertRaises(ValueError):write_bundle(self.source,self.manifest,self.output)
        self.assertEqual((self.output/'manifest.json').read_bytes(),saved)
        verify_bundle(self.output)
    def test_output_unknown_files_are_preserved_and_rejected(self):
        self.output.mkdir();user=self.output/'user.txt';user.write_text('keep',encoding='utf-8')
        with self.assertRaisesRegex(ValueError,'owned'):write_bundle(self.source,self.manifest,self.output)
        self.assertEqual(user.read_text(encoding='utf-8'),'keep')
    def test_source_path_escape_is_rejected(self):
        (self.root/'outside.hpp').write_text('secret',encoding='utf-8')
        self.manifest['items'][0]['implementation']['interfaceFiles']=['../outside.hpp']
        with self.assertRaisesRegex(ValueError,'path'):write_bundle(self.source,self.manifest,self.output)
    def test_missing_and_extra_output_files_are_rejected(self):
        write_bundle(self.source,self.manifest,self.output)
        header=self.output/'src/Service.hpp';saved=header.read_bytes();header.unlink()
        with self.assertRaisesRegex(ValueError,'inventory'):verify_bundle(self.output)
        header.write_bytes(saved);(self.output/'extra.txt').write_text('extra',encoding='utf-8')
        with self.assertRaisesRegex(ValueError,'inventory'):verify_bundle(self.output)
if __name__=='__main__':unittest.main()
