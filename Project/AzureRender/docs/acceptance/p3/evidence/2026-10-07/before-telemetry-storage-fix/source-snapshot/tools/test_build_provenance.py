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


if __name__ == "__main__":
    unittest.main()
