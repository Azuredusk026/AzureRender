"""Check byte-exact recovery, dirty inputs, and guarded archive removal."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

TOOL = Path(__file__).with_name('evidence_archive.py')


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        (self.root/'.gitignore').write_text('build/\n')
        (self.root/'src').mkdir()
        (self.root/'src/input.cpp').write_text('original\n')
        subprocess.run(['git', '-C', str(self.root), 'add', '.'], check=True)
        subprocess.run(['git', '-C', str(self.root), '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid', 'commit', '-qm', 'seed'], check=True)
        self.source = self.root/'build/runs/closed'
        self.source.mkdir(parents=True)
        (self.source/'.finished').touch()
        (self.source/'raw.bin').write_bytes(b'\0\xff\r\nexact bytes')
        (self.source/'receipt.json').write_text('{"result":"passed"}')

    def cli(self, *args):
        r = subprocess.run([sys.executable, str(TOOL), '--root', str(self.root), *args], capture_output=True, encoding='utf-8')
        self.assertTrue(r.stdout.strip(), r.stderr)
        return r.returncode, json.loads(r.stdout)

    def test_archive_roundtrip_preserves_binary_and_uncommitted_inputs(self):
        (self.root/'src/input.cpp').write_bytes(b'changed\r\n')
        (self.root/'src/new.hpp').write_bytes(b'untracked\0input')
        code, data = self.cli('pack', '--source', 'build/runs/closed', '--output', 'build/archives/proof.zip', '--input', 'src/input.cpp', '--input', 'src/new.hpp')
        self.assertEqual(code, 0)
        self.assertTrue((self.source/'raw.bin').exists())
        code, restored = self.cli('restore', '--archive', 'build/archives/proof.zip', '--output', 'build/runs/restored')
        self.assertEqual(code, 0)
        output = self.root/'build/runs/restored'
        self.assertEqual((output/'payload/raw.bin').read_bytes(), b'\0\xff\r\nexact bytes')
        self.assertEqual((output/'inputs/src/input.cpp').read_bytes(), b'changed\r\n')
        self.assertEqual((output/'inputs/src/new.hpp').read_bytes(), b'untracked\0input')
        self.assertIn('changed', (output/'working-tree.patch').read_text())

    def test_removal_requires_finished_unpinned_untracked_source(self):
        (self.source/'.keep').touch()
        self.assertEqual(self.cli('pack', '--source', 'build/runs/closed', '--output', 'build/proof.zip', '--remove-source')[0], 2)
        (self.source/'.keep').unlink()
        (self.source/'.finished').unlink()
        self.assertEqual(self.cli('pack', '--source', 'build/runs/closed', '--output', 'build/proof.zip', '--remove-source')[0], 2)
        (self.source/'.finished').touch()
        self.assertEqual(self.cli('pack', '--source', 'build/runs/closed', '--output', 'build/proof.zip', '--remove-source')[0], 0)
        self.assertFalse(self.source.exists())

    def test_manifest_hash_detects_corruption_and_zip_slip(self):
        self.assertEqual(self.cli('pack', '--source', 'build/runs/closed', '--output', 'build/proof.zip')[0], 0)
        with zipfile.ZipFile(self.root/'build/proof.zip') as z:
            manifest = z.read('manifest.json')
            members = [(i.filename,z.read(i)) for i in z.infolist() if i.filename != 'manifest.json']
        with zipfile.ZipFile(self.root/'build/tampered.zip','w') as z:
            z.writestr('manifest.json', manifest)
            for name,data in members:z.writestr(name,b'corrupt' if name.endswith('raw.bin') else data)
        self.assertEqual(self.cli('verify','--archive','build/tampered.zip')[0], 2)
        with zipfile.ZipFile(self.root/'build/evil.zip','w') as z:
            z.writestr('manifest.json',json.dumps({'schemaVersion':1,'files':[]}))
            z.writestr('../escape.txt','bad')
        self.assertEqual(self.cli('restore','--archive','build/evil.zip','--output','build/runs/evil')[0], 2)
        self.assertFalse((self.root/'build/escape.txt').exists())

    def test_restore_refuses_existing_destination_and_traversal(self):
        self.assertEqual(self.cli('pack','--source','build/runs/closed','--output','build/proof.zip')[0],0)
        self.assertEqual(self.cli('restore','--archive','build/proof.zip','--output','build/runs/closed')[0],2)
        self.assertEqual(self.cli('pack','--source','../outside','--output','build/no.zip')[0],2)

    def test_tracked_evidence_is_never_removed(self):
        subprocess.run(['git','-C',str(self.root),'add','-f','build/runs/closed/raw.bin'],check=True)
        self.assertEqual(self.cli('pack','--source','build/runs/closed','--output','build/proof.zip','--remove-source')[0],2)
        self.assertTrue((self.source/'raw.bin').exists())

    def test_nested_engine_root_uses_repository_relative_tracked_paths(self):
        engine=self.root/'Project/AzureRender'
        source=engine/'build/runs/tracked';source.mkdir(parents=True)
        (source/'.finished').touch();(source/'raw.bin').write_bytes(b'keep')
        subprocess.run(['git','-C',str(self.root),'add','-f','Project/AzureRender/build/runs/tracked/raw.bin'],check=True)
        result=subprocess.run([sys.executable,str(TOOL),'--root',str(engine),'pack','--source','build/runs/tracked','--output','build/proof.zip','--remove-source'],capture_output=True)
        self.assertEqual(result.returncode,2)
        self.assertTrue((source/'raw.bin').exists())


if __name__ == '__main__':unittest.main()
