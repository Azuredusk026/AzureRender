"""Reject stale source and altered build products."""
import json
from pathlib import Path
import tempfile
import unittest
import build_provenance


class ProvenanceTests(unittest.TestCase):
    def test_adoption_manifest_is_a_build_input(self):
        with tempfile.TemporaryDirectory() as temp:
            source=Path(temp)
            plan=source/'docs/plans/engine-evolution-manifest.json'
            plan.parent.mkdir(parents=True);plan.write_text('{"schemaVersion":1}')
            manifest=build_provenance.describe(source,[])
            self.assertIn('docs/plans/engine-evolution-manifest.json',manifest['source'])
            plan.write_text('{"schemaVersion":2}')
            with self.assertRaisesRegex(ValueError,'source'):build_provenance.verify(source,manifest)

    def test_contract_snapshot_inventory_and_bytes_are_build_products(self):
        from write_engine_contracts import write_bundle
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);source=root/'source';source.mkdir()
            header=source/'src/Contract.hpp';header.parent.mkdir();header.write_bytes(b'struct Contract {};\n')
            adoption={'referenceCommit':'fixture','items':[{'id':'A1','implementation':{
                'interfaceFiles':['src/Contract.hpp'],'interfaces':['Contract'],'scope':'Fixture',
                'verification':{'adoption':'adapted-adoption'}}}]}
            output=root/'contracts';write_bundle(source,adoption,output)
            products=build_provenance.product_paths(root,'Release')
            self.assertIn(output/'manifest.json',products)
            self.assertIn(output/'src/Contract.hpp',products)
            for product in products:
                if not product.exists():product.write_bytes(b'compiled product')
            manifest=build_provenance.describe(source,products);build_provenance.verify(source,manifest)
            (output/'extra.hpp').write_bytes(b'unknown contract')
            with self.assertRaisesRegex(ValueError,'inventory'):build_provenance.verify(source,manifest)
            (output/'extra.hpp').unlink();(output/'src/Contract.hpp').write_bytes(b'altered')
            with self.assertRaisesRegex(ValueError,'differs'):build_provenance.verify(source,manifest)

    def test_managed_module_tampering_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);(root/'CMakeCache.txt').write_text('AZURE_ENABLE_MANAGED_SCRIPT_PROTOTYPE:BOOL=ON\n')
            native=root/'managed/nativeaot/Azure.Engine.Native.dll';native.parent.mkdir(parents=True);native.write_bytes(b'compiled managed module')
            products=build_provenance.product_paths(root,'Debug')
            self.assertIn(native,products)
            for product in products:
                product.parent.mkdir(parents=True,exist_ok=True)
                if not product.exists():product.write_bytes(b'build product')
            source=root/'source';source.mkdir()
            manifest=build_provenance.describe(source,products);build_provenance.verify(source,manifest)
            native.write_bytes(b'altered native module')
            with self.assertRaisesRegex(ValueError,'Azure.Engine.Native.dll'):build_provenance.verify(source,manifest)
    def test_geometry_compiler_product_tampering_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            products = build_provenance.product_paths(root, "Debug")
            self.assertEqual({p.stem for p in products},
                {"AzureRender", "AzurePlayer", "AzureMetaGen", "AzureGeometryCompiler"})
            for product in products:
                product.write_bytes(b"verified executable")
            manifest = build_provenance.describe(root, products)
            build_provenance.verify(root, manifest)
            compiler = next(p for p in products if p.stem == "AzureGeometryCompiler")
            compiler.write_bytes(b"altered compiler")
            with self.assertRaisesRegex(ValueError, "AzureGeometryCompiler"):
                build_provenance.verify(root, manifest)

    def test_exchange_tool_tampering_is_rejected(self):
        import os
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            tool=root/('AzureAssetExchange.exe' if os.name=='nt' else 'AzureAssetExchange')
            tool.write_bytes(b'original exchange')
            products=build_provenance.product_paths(root,'Release')
            self.assertIn(tool,products)
            for product in products:
                if not product.exists():product.write_bytes(b'compiled executable')
            manifest=build_provenance.describe(root,products)
            tool.write_bytes(b'altered exchange')
            with self.assertRaisesRegex(ValueError,'AzureAssetExchange'):
                build_provenance.verify(root,manifest)

    def test_rejects_changed_source_and_binary(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "src").mkdir()
            source = root / "src/main.cpp"
            source.write_text("int main() { return 0; }")
            binary = root / "player.exe"
            binary.write_bytes(b"original product")
            manifest = build_provenance.describe(root, [binary])
            build_provenance.verify(root, manifest)
            source.write_text("int main() { return 1; }")
            with self.assertRaisesRegex(ValueError, "source"):
                build_provenance.verify(root, manifest)
            source.write_text("int main() { return 0; }")
            binary.write_bytes(b"altered product")
            with self.assertRaisesRegex(ValueError, "product"):
                build_provenance.verify(root, manifest)

    def test_missing_binary_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(FileNotFoundError):
                build_provenance.describe(Path(temp), [Path(temp) / "missing.exe"])


class ScopedProvenanceTests(unittest.TestCase):
    def setUp(self):
        import os
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.source=self.root/'source';self.build=self.root/'build'
        (self.source/'src').mkdir(parents=True);self.build.mkdir()
        (self.source/'src/Runtime.hpp').write_text('struct RuntimeFixture {};\n')
        (self.source/'CMakeLists.txt').write_text('# isolated orchestration fixture\n')
        (self.build/'CMakeCache.txt').write_text('AZURE_ENABLE_MANAGED_SCRIPT_PROTOTYPE:BOOL=OFF\n')
        self.suffix='.exe' if os.name=='nt' else '';self.build_calls=[];self.omit=set();self.fail_build=False;self.mutate=False
    def runner(self,command,**kwargs):
        import subprocess
        if command[0]=='git':
            return subprocess.CompletedProcess(command,0,stdout='fixture\n' if 'rev-parse' in command else '',stderr='')
        self.assertEqual(command[0],'cmake');self.build_calls.append(command)
        if self.fail_build:
            if kwargs.get('check'):raise subprocess.CalledProcessError(1,command)
            return subprocess.CompletedProcess(command,1)
        if '--target' in command:targets=command[command.index('--target')+1:]
        else:targets=['AzureRender','AzurePlayer','AzureMetaGen','AzureGeometryCompiler','AzureAssetExchange','AzureMaterialValidator','AzureGameLauncher','AzureEngineContracts','AzureManagedArtifacts','AzureShaderModulePrototypes','SelectedTests','UnrelatedSlowTests']
        for name in targets:
            if name in self.omit:continue
            if name=='AzureEngineContracts':
                from write_engine_contracts import write_bundle
                adoption={'referenceCommit':'fixture','items':[{'id':'S1','implementation':{'interfaceFiles':['src/Runtime.hpp'],'interfaces':['RuntimeFixture'],'scope':'Fixture','verification':{'adoption':'adapted-adoption'}}}]}
                write_bundle(self.source,adoption,self.build/'contracts')
            elif name=='AzureShaderModulePrototypes':
                for item in ['bloom-direct.spv','bloom-direct.spv.json','bloom-cached.spv','bloom-cached.spv.json']:
                    f=self.build/'shader-modules'/item;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(b'fixture shader module')
            elif name=='AzureManagedArtifacts':
                for item in ['coreclr/Azure.Engine.dll','coreclr/Azure.Engine.runtimeconfig.json','coreclr/Azure.Engine.deps.json','coreclr/Azure.Samples.dll','nativeaot/Azure.Engine.Native.dll','manifest.json','complete.stamp']:
                    f=self.build/'managed'/item;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(b'built managed fixture')
            else:(self.build/(name+self.suffix)).write_bytes(('built:'+name).encode())
        if self.mutate:(self.source/'src/Runtime.hpp').write_text('struct RuntimeFixture { int changed; };')
        return subprocess.CompletedProcess(command,0)
    def invoke(self,*flags,mode='build'):
        import io,sys
        from unittest import mock
        argv=['build_provenance.py',mode,'--source',str(self.source),'--build-dir',str(self.build),'--config','Release',*flags]
        with mock.patch.object(sys,'argv',argv),mock.patch.object(build_provenance.subprocess,'run',side_effect=self.runner),mock.patch('sys.stdout',new=io.StringIO()),mock.patch('sys.stderr',new=io.StringIO()):
            build_provenance.main()
        return json.loads((self.build/'build-provenance.json').read_text())
    def test_runtime_and_selected_tests_are_frozen_without_unrelated_work(self):
        (self.build/'CMakeCache.txt').write_text('AZURE_ENABLE_MANAGED_SCRIPT_PROTOTYPE:BOOL=ON\n')
        manifest=self.invoke('--test-target','SelectedTests','--test-target','SelectedTests')
        for name in ['AzureRender','AzurePlayer','AzureMetaGen','AzureGeometryCompiler','AzureAssetExchange','SelectedTests']:
            self.assertIn(str((self.build/(name+self.suffix)).resolve()),manifest['products'])
        self.assertIn(str((self.build/'managed/nativeaot/Azure.Engine.Native.dll').resolve()),manifest['products'])
        self.assertIn(str((self.build/'contracts/manifest.json').resolve()),manifest['products'])
        self.assertFalse((self.build/('UnrelatedSlowTests'+self.suffix)).exists())
        self.assertEqual(manifest['buildScope']['kind'],'runtime-and-tests')
        self.assertEqual(self.build_calls[0].count('SelectedTests'),1)
        build_provenance.verify(self.source,manifest)
    def test_runtime_only_build_preserves_runtime_and_contract_inventory(self):
        manifest=self.invoke('--runtime-only')
        self.assertIn(str((self.build/('AzureAssetExchange'+self.suffix)).resolve()),manifest['products'])
        self.assertIn(str((self.build/'contracts/src/Runtime.hpp').resolve()),manifest['products'])
        self.assertFalse((self.build/('SelectedTests'+self.suffix)).exists())
        self.assertEqual(manifest['buildScope']['kind'],'runtime')
    def test_default_build_still_builds_the_complete_configured_set(self):
        manifest=self.invoke()
        self.assertTrue((self.build/('UnrelatedSlowTests'+self.suffix)).exists())
        self.assertNotIn('--target',self.build_calls[0]);self.assertEqual(manifest['buildScope']['kind'],'all')
    def test_selected_binary_tampering_is_rejected_without_rebuilding(self):
        self.invoke('--test-target','SelectedTests');self.invoke(mode='verify')
        self.assertEqual(len(self.build_calls),1)
        (self.build/('SelectedTests'+self.suffix)).write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError,'SelectedTests'):self.invoke(mode='verify')
    def test_native_tools_and_shader_products_are_hashed(self):
        shaders=self.build/'shaders';shaders.mkdir();program=shaders/'surface.frag.spv';program.write_bytes(b'fixture shader')
        manifest=self.invoke('--runtime-only')
        for name in ['AzureMaterialValidator']+(['AzureGameLauncher'] if self.suffix else []):
            self.assertIn(str((self.build/(name+self.suffix)).resolve()),manifest['products'])
        self.assertIn(str(program.resolve()),manifest['products'])
        program.write_bytes(b'altered shader')
        with self.assertRaisesRegex(ValueError,'surface.frag.spv'):build_provenance.verify(self.source,manifest)
    def test_enabled_shader_module_artifacts_are_built_and_frozen(self):
        (self.build/'CMakeCache.txt').write_text('AZURE_ENABLE_SHADER_MODULE_PROTOTYPE:BOOL=ON\n')
        manifest=self.invoke('--runtime-only');module=self.build/'shader-modules/bloom-cached.spv'
        self.assertIn(str(module.resolve()),manifest['products']);module.write_bytes(b'altered module')
        with self.assertRaisesRegex(ValueError,'bloom-cached.spv'):build_provenance.verify(self.source,manifest)
    def test_fingerprint_uses_bounded_memory_for_large_files(self):
        import hashlib,tracemalloc
        source=self.source/'large-input.bin';block=b'0123456789abcdef'*256;expected=hashlib.sha256()
        with source.open('wb') as stream:
            for _ in range(8192):stream.write(block);expected.update(block)
        tracemalloc.start()
        try:actual=build_provenance.digest(source);_,peak=tracemalloc.get_traced_memory()
        finally:tracemalloc.stop()
        self.assertEqual(actual,expected.hexdigest());self.assertLess(peak,4*1024*1024,'Fingerprint working memory must stay bounded for large assets')
    def test_scoped_receipt_rejects_omitted_selected_product(self):
        manifest=self.invoke('--test-target','SelectedTests')
        manifest['products'].pop(str((self.build/('SelectedTests'+self.suffix)).resolve()))
        with self.assertRaisesRegex(ValueError,'Scoped test product'):
            build_provenance.verify(self.source,manifest)
    def test_missing_selected_or_runtime_product_cannot_publish_a_manifest(self):
        for missing in ['SelectedTests','AzurePlayer']:
            with self.subTest(missing=missing):
                self.omit={missing};f=self.build/(missing+self.suffix)
                if f.exists():f.unlink()
                with self.assertRaises(FileNotFoundError):self.invoke('--test-target','SelectedTests')
                self.assertFalse((self.build/'build-provenance.json').exists())
    def test_source_mutation_and_build_failure_preserve_prior_receipt(self):
        import subprocess
        manifest=self.build/'build-provenance.json';manifest.write_text('{"prior":true}')
        self.fail_build=True
        with self.assertRaises(subprocess.CalledProcessError):self.invoke('--runtime-only')
        self.assertEqual(json.loads(manifest.read_text()),{'prior':True})
        self.fail_build=False;self.mutate=True
        with self.assertRaisesRegex(ValueError,'source changed'):self.invoke('--runtime-only')
        self.assertEqual(json.loads(manifest.read_text()),{'prior':True})
    def test_invalid_target_and_verify_scope_options_fail_before_build(self):
        for flags,mode in [(('--test-target','../outside'),'build'),(('--test-target','--clean-first'),'build'),(('--runtime-only',),'verify'),(('--test-target','SelectedTests'),'verify')]:
            with self.subTest(flags=flags,mode=mode):
                with self.assertRaises(SystemExit):self.invoke(*flags,mode=mode)
        self.assertFalse(self.build_calls)


if __name__ == "__main__":
    unittest.main()
