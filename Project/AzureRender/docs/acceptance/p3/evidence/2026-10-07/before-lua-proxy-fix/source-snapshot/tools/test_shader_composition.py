"""Compile interchangeable strategies and reject incompatible sample types."""
from pathlib import Path
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from test_shader_module import COMPILER,ROOT


class CompositionTests(unittest.TestCase):
    def test_both_strategies_and_incompatible_interface(self):
        with tempfile.TemporaryDirectory(prefix='azure shader composition ') as folder:
            root=Path(folder);shutil.copytree(ROOT/'shaders/modules',root/'modules')
            output=root/'shader.spv';request=root/'request.json'
            parameters=dict(schemaVersion=1,source=str(root/'modules/Bloom.slang'),entry='main',target='spirv',
                profile='spirv_1_3',compiler=str(COMPILER),includeRoots=[str(root/'modules')],
                layout=str(root/'modules/shared-layout.json'),defines={},output=str(output))
            def compile():
                request.write_text(json.dumps(parameters))
                return subprocess.run([sys.executable,str(ROOT/'tools/compile_shader_module.py'),'--request',str(request)],
                    capture_output=True,text=True,encoding='utf-8',errors='replace')
            for mode in ['0','1']:
                parameters['defines']={'AZURE_BLOOM_CACHE':mode}
                result=compile();self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual(output.read_bytes()[:4],b'\x03\x02\x23\x07')
            original=output.read_bytes()
            interface=root/'modules/interfaces/BlockSamples.slang'
            interface.write_text(interface.read_text().replace('float3 read','float2 read'))
            result=compile()
            self.assertNotEqual(result.returncode,0,'Strategies must satisfy the sample interface')
            self.assertIn('float,3',result.stderr.lower())
            self.assertIn('float,2',result.stderr.lower())
            self.assertEqual(output.read_bytes(),original)


if __name__=='__main__':unittest.main()
