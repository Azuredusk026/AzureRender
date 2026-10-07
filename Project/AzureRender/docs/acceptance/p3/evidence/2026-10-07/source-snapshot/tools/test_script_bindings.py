"""Run the binding generator against valid, invalid and drifted artifacts."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class ScriptBindingGenerationTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory(prefix='azure_script_bindings_')
        self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name)
        self.schema=self.root/'bindings.json'
        self.description=json.loads((ROOT/'schemas/script_bindings.json').read_text())
        self.schema.write_text(json.dumps(self.description))
        self.output=self.root/'generated'

    def run_generator(self,*flags):
        return subprocess.run([sys.executable,str(ROOT/'tools/generate_script_bindings.py'),
            '--schema',str(self.schema),'--output-root',str(self.output),*flags],capture_output=True,text=True)

    def test_check_rejects_drift_without_rewriting(self):
        generated=self.run_generator()
        self.assertEqual(generated.returncode,0,generated.stderr)
        self.assertEqual(self.run_generator('--check').returncode,0)
        header=self.output/'src/scripting/GeneratedBindings.hpp'
        corrupted=header.read_bytes()+b'\n// corrupted artifact\n'
        header.write_bytes(corrupted)
        self.assertNotEqual(self.run_generator('--check').returncode,0)
        self.assertEqual(header.read_bytes(),corrupted)

    def test_invalid_version_and_duplicate_methods_leave_no_output(self):
        for mutation in ('version','duplicate'):
            with self.subTest(mutation=mutation):
                document=json.loads(json.dumps(self.description))
                if mutation=='version':document['apiVersion']=99
                else:document['methods'].append(document['methods'][0])
                self.schema.write_text(json.dumps(document))
                self.assertEqual(self.run_generator().returncode,3)
                self.assertFalse(self.output.exists())

    def test_unknown_type_and_writable_identity_are_rejected(self):
        for mutation in ('type','permission'):
            with self.subTest(mutation=mutation):
                document=json.loads(json.dumps(self.description))
                if mutation=='type':document['methods'][0]['parameters'][0]['type']='pointer'
                else:document['properties'][0]['access']='write'
                self.schema.write_text(json.dumps(document))
                self.assertEqual(self.run_generator().returncode,3)
                self.assertFalse(self.output.exists())

if __name__=='__main__':unittest.main()
