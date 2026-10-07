"""Reject generated host/shader ABI drift from a single checked description."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


class SharedTypeTests(unittest.TestCase):
    def test_generate_check_and_detect_independent_tampering(self):
        with tempfile.TemporaryDirectory(prefix='azure shader layout ') as folder:
            root=Path(folder);description=root/'layout.json';cpp=root/'types.hpp';slang=root/'Types.slang'
            description.write_text(json.dumps(dict(schemaVersion=1,types=[dict(name='Probe',size=8,alignment=4,fields=[
                dict(name='value',type='float32',offset=0,default=1.2),dict(name='mode',type='uint32',offset=4,default=0)])])))
            command=[sys.executable,str(ROOT/'tools/generate_shader_types.py'),'--description',str(description),'--cpp',str(cpp),'--slang',str(slang)]
            result=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertIn('offsetof(Probe, mode) == 4',cpp.read_text())
            self.assertIn('public uint mode;',slang.read_text())
            self.assertEqual(subprocess.run(command+['--check'],capture_output=True).returncode,0)
            for path in [cpp,slang]:
                original=path.read_bytes();path.write_bytes(original+b'\n// drift\n')
                self.assertNotEqual(subprocess.run(command+['--check'],capture_output=True).returncode,0)
                path.write_bytes(original)


if __name__=='__main__': unittest.main()
