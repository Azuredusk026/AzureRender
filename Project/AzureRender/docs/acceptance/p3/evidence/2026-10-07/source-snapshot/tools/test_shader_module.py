"""Exercise the real shader compiler, dependency graph and reflected ABI."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import shutil
from unittest.mock import patch
import compile_shader_module as adapter

ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path(os.environ.get('AZURE_SLANG_COMPILER', 'C:/VulkanSDK/1.4.350.0/Bin/slangc.exe'))


class ShaderModuleTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='azure shader contract ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root/'Types.slang').write_text('public struct Parameters { public float threshold; public uint extractBright; };')
        (self.root/'main.slang').write_text('''import Types;
[[vk::push_constant]] ConstantBuffer<Parameters> parameters;
[[vk::binding(0,0)]] RWStructuredBuffer<float> output;
[shader("compute")] [numthreads(8,8,1)]
void main(uint3 tid : SV_DispatchThreadID) { output[tid.x] = parameters.threshold + parameters.extractBright; }
''')
        self.layout = dict(schemaVersion=1, types=[dict(name='Parameters', size=8, alignment=4,
            fields=[dict(name='threshold', type='float32', offset=0, default=1.2),
                    dict(name='extractBright', type='uint32', offset=4, default=0)])])
        (self.root/'layout.json').write_text(json.dumps(self.layout))
        self.request = dict(schemaVersion=1, source=str(self.root/'main.slang'), entry='main',
            target='spirv', profile='spirv_1_3', compiler=str(COMPILER),
            includeRoots=[str(self.root)], layout=str(self.root/'layout.json'), defines={},
            output=str(self.root/'candidate.spv'))

    def compile(self):
        path = self.root/'request.json'
        path.write_text(json.dumps(self.request), encoding='utf-8')
        return subprocess.run([sys.executable, str(ROOT/'tools/compile_shader_module.py'), '--request', str(path)],
            capture_output=True, text=True, encoding='utf-8', errors='replace')

    def success(self):
        result = self.compile()
        self.assertEqual(result.returncode, 0, 'The compiler adapter must produce valid SPIR-V: '+result.stderr)
        output = self.root/'candidate.spv'
        self.assertEqual(output.read_bytes()[:4], b'\x03\x02\x23\x07')
        return json.loads(output.with_suffix('.spv.json').read_text())

    def test_real_compiler_reflection_and_incremental_dependencies(self):
        first = self.success()
        self.assertFalse(first['cacheHit'])
        self.assertEqual(len(first['inputs']), 3)
        second = self.success()
        self.assertTrue(second['cacheHit'])
        source = self.root/'Types.slang'
        source.write_text(source.read_text()+'\n// dependency edit\n')
        third = self.success()
        self.assertFalse(third['cacheHit'])
        self.assertNotEqual(first['inputHash'], third['inputHash'])

    def test_missing_compiler_is_diagnostic(self):
        self.request['compiler'] = str(self.root/'missing.exe')
        result = self.compile()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('compiler', result.stderr.lower())
        self.assertFalse((self.root/'candidate.spv').exists())

    def test_layout_member_mismatch_preserves_valid_output(self):
        self.success()
        previous = (self.root/'candidate.spv').read_bytes()
        self.layout['types'][0]['fields'][1]['offset'] = 0
        (self.root/'layout.json').write_text(json.dumps(self.layout))
        result = self.compile()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('layout', result.stderr.lower())
        self.assertEqual((self.root/'candidate.spv').read_bytes(), previous)

    def test_missing_dependency_invalid_entry_and_target(self):
        for name,value in [('entry','missing'),('target','unsupported')]:
            with self.subTest(name=name):
                saved = self.request[name]
                self.request[name] = value
                self.assertNotEqual(self.compile().returncode, 0)
                self.request[name] = saved
        (self.root/'Types.slang').unlink()
        result = self.compile()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('dependency', result.stderr.lower())

    def test_structure_span_and_scalar_types_are_reflected(self):
        (self.root/'main.slang').write_text('''import Types;
[[vk::binding(0,0)]] StructuredBuffer<Parameters> input;
[[vk::binding(1,0)]] RWStructuredBuffer<float> output;
[shader("compute")] [numthreads(8,8,1)]
void main(uint3 tid : SV_DispatchThreadID) { output[tid.x] = input[tid.x].threshold + input[tid.x].extractBright; }
''')
        result = self.success()
        self.assertEqual(result['validatedTypes'], ['Parameters'])
        self.assertEqual(result['structuredStrides'], {'Parameters':8})
        (self.root/'Types.slang').write_text('public struct Parameters { public uint threshold; public uint extractBright; };')
        failure = self.compile()
        self.assertNotEqual(failure.returncode, 0)
        self.assertIn('layout', failure.stderr.lower())

    def test_dependency_cycle_and_unknown_request_fields(self):
        (self.root/'Types.slang').write_text('import main;\npublic struct Parameters { public float threshold; public uint extractBright; };')
        result = self.compile()
        self.assertNotEqual(result.returncode,0)
        self.assertIn('cycle',result.stderr.lower())
        self.request['unrecognized'] = 1
        result = self.compile()
        self.assertNotEqual(result.returncode,0)
        self.assertIn('field',result.stderr.lower())

    def test_tampered_cached_reflection_is_rejected(self):
        self.success()
        path=self.root/'candidate.spv.json'
        value=json.loads(path.read_text())
        value['reflection']['parameters'][0]['type']['elementType']['fields'][0]['binding']['offset']=4
        path.write_text(json.dumps(value))
        result=self.compile()
        self.assertNotEqual(result.returncode,0,'Cached reflection must satisfy the same layout contract')
        self.assertIn('layout',result.stderr.lower())

    def test_contract_versions_require_exact_integer_types(self):
        for version in [True,1.0,'1',2]:
            with self.subTest(version=version):
                self.request['schemaVersion']=version
                self.assertNotEqual(self.compile().returncode,0,'Request version must be the integer 1')

    def test_metadata_install_failure_restores_binary_and_metadata(self):
        for existing in [False, True]:
            with self.subTest(existing=existing):
                output=self.root/'candidate.spv'
                metadata=self.root/'candidate.spv.json'
                if existing:
                    self.success()
                old={p:p.read_bytes() if p.exists() else None for p in [output,metadata]}
                source=self.root/'main.slang'
                source.write_text(source.read_text().replace('parameters.threshold', '2 * parameters.threshold'))
                replace=adapter.os.replace
                def fail_metadata_once(source,target):
                    if Path(source).name=='metadata.json' and Path(target)==metadata:
                        raise PermissionError('injected metadata installation failure')
                    return replace(source,target)
                with patch.object(adapter.os,'replace',side_effect=fail_metadata_once):
                    with self.assertRaises(PermissionError):
                        adapter.compile_module(adapter.ShaderCompileRequest.parse(self.request))
                for path,previous in old.items():
                    self.assertEqual(path.read_bytes() if path.exists() else None,previous,
                        'A failed publication must restore both prior artifacts')

    def test_output_must_preserve_all_input_files(self):
        for name in ['main.slang','Types.slang','layout.json']:
            with self.subTest(name=name):
                path=self.root/name
                previous=path.read_bytes()
                self.request['output']=str(path)
                result=self.compile()
                self.assertNotEqual(result.returncode,0,'Input aliases must be rejected before publication')
                self.assertEqual(path.read_bytes(),previous)

    def test_compiler_library_change_invalidates_cache(self):
        compiler_root=self.root/'compiler'
        compiler_root.mkdir()
        files=[COMPILER,*COMPILER.parent.glob('slang*.dll')]
        self.assertTrue(any(p.name=='slang-compiler.dll' for p in files))
        for path in files: shutil.copy2(path,compiler_root/path.name)
        self.request['compiler']=str(compiler_root/COMPILER.name)
        self.assertFalse(self.success()['cacheHit'])
        self.assertTrue(self.success()['cacheHit'])
        library=compiler_root/'slang-compiler.dll'
        # PE permits an overlay after its executable sections. Preserve the
        # real compiler's behavior while changing its actual library identity.
        with library.open('ab') as file: file.write(b'azure cache identity probe')
        rebuilt=self.success()
        self.assertFalse(rebuilt['cacheHit'],'Compiler library identity must invalidate the cache')
        self.assertEqual(rebuilt['compilerArtifacts'][str(library)],
            __import__('hashlib').sha256(library.read_bytes()).hexdigest())


if __name__ == '__main__':
    unittest.main()
