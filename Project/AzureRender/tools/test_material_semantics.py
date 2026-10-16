import copy
import importlib.util
from pathlib import Path
import unittest
import tempfile
import subprocess
import sys


class MaterialSemanticsTests(unittest.TestCase):
    def setUp(self):
        path = Path(__file__).with_name('convert_material_semantics.py')
        self.assertTrue(path.exists(), 'Material conversion implementation is missing')
        spec = importlib.util.spec_from_file_location('semantics', path)
        self.tool = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.tool)

    def test_restore_source_recovers_images_lost_by_blender(self):
        import json,hashlib,base64
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);image=root/'source.png';image.write_bytes(b'original-image')
            source=root/'source.gltf';doc={'materials':[{'name':'Face','normalTexture':{'index':0}}],
                'textures':[{'source':0,'sampler':0}],'samplers':[{'wrapS':33071}],'images':[{'uri':'source.png'}]}
            source.write_text(json.dumps(doc));mapping=self.tool.describe(doc)
            mapping['source']={'sha256':hashlib.sha256(source.read_bytes()).hexdigest()}
            result=self.tool.restore_with_source({'materials':[{'name':'Face'}]},mapping,source)
            self.assertEqual(result['materials'][0]['normalTexture']['index'],0)
            self.assertEqual(result['samplers'],doc['samplers'])
            self.assertEqual(base64.b64decode(result['images'][0]['uri'].split(',')[1]),b'original-image')
            self.assertEqual(doc['images'][0]['uri'],'source.png')
            source.write_text('{}')
            with self.assertRaisesRegex(ValueError,'fingerprint'):
                self.tool.restore_with_source({'materials':[{'name':'Face'}]},mapping,source)

    def test_packed_ao_roughness_and_emission_remain_distinct(self):
        source = {'materials': [{'name': 'M', 'pbrMetallicRoughness': {'metallicRoughnessTexture': {'index': 0}},
                 'emissiveTexture': {'index': 1}, 'occlusionTexture': {'index': 2}}],
                 'textures': [{'source': 0}, {'source': 1}, {'source': 2}], 'images': [{}, {}, {}]}
        result = self.tool.describe(source)['materials'][0]
        self.assertEqual(result['channels']['roughness'], {'texture': 0, 'channel': 'g', 'colorSpace': 'linear'})
        self.assertEqual(result['channels']['metallic']['channel'], 'b')
        self.assertEqual(result['channels']['emissive'], {'texture': 1, 'channel': 'rgb', 'colorSpace': 'srgb'})
        self.assertEqual(result['channels']['occlusion_source']['texture'], 2)
        self.assertIn('occlusion_is_style_mask', [loss['code'] for loss in result['losses']])

    def test_metadata_restores_by_stable_slot_after_reorder(self):
        source = {'materials': [{'name': 'Face', 'extras': {'azureRenderMaterial': {'schemaVersion': 1, 'class': 'face'}, 'afterglowHairParameters': [1, 2, 3, 4]}}, {'name': 'Body'}]}
        mapping = self.tool.describe(source)
        exported = {'materials': [{'name': 'Body'}, {'name': 'Face'}]}
        restored = self.tool.restore(exported, mapping)
        self.assertEqual(restored['materials'][1]['extras']['azureRenderMaterial']['class'], 'face')
        self.assertEqual(restored['materials'][1]['extras']['azureSlotId'], 'name:Face')
        self.assertNotIn('extras', exported['materials'][1])

    def test_duplicate_explicit_slots_are_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Duplicate'):
            self.tool.describe({'materials': [{'extras': {'azureSlotId': 'same'}}, {'extras': {'azureSlotId': 'same'}}]})

    def test_count_mismatch_and_missing_slot_are_rejected(self):
        with self.assertRaisesRegex(ValueError, 'count'):
            self.tool.describe({'materials': []}, expected_count=13)
        with self.assertRaisesRegex(ValueError, 'slot'):
            self.tool.restore({'materials': [{'name': 'Other'}]}, self.tool.describe({'materials': [{'name': 'Original'}]}))

    def test_unknown_texture_reference_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'texture'):
            self.tool.describe({'materials': [{'normalTexture': {'index': 4}}]})

    def test_generic_document_uses_registered_fields_and_uuid_textures(self):
        self.assertTrue(hasattr(self.tool, 'material_document'))
        result = self.tool.material_document({'pbrMetallicRoughness': {'roughnessFactor': .4, 'baseColorTexture': {'index': 2}}},
            {'2': '12345678-1234-1234-1234-123456789abc'})
        self.assertEqual(result['typeId'], 'azure.pbr')
        self.assertEqual(result['parameters']['roughness'], .4)
        self.assertEqual(result['textures']['albedo'], {'asset': '12345678-1234-1234-1234-123456789abc', 'usage': 'albedo'})
        self.assertEqual(len(result['parameters']), 10)

    def test_specialized_profile_and_unresolved_texture_reject_override(self):
        self.assertTrue(hasattr(self.tool, 'material_document'))
        with self.assertRaisesRegex(ValueError, 'specialized'):
            self.tool.material_document({'extras': {'azureRenderMaterial': {'class': 'face'}}}, {})
        with self.assertRaisesRegex(ValueError, 'UUID'):
            self.tool.material_document({'normalTexture': {'index': 0}}, {})

    def test_report_cannot_overwrite_source(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source.gltf'
            raw = b'{"materials": []}'
            source.write_bytes(raw)
            result = subprocess.run([sys.executable, str(Path(__file__).with_name('convert_material_semantics.py')),
                '--input', str(source), '--output', str(source)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(source.read_bytes(), raw)


if __name__ == '__main__':
    unittest.main()
