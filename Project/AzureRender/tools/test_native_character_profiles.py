"""Verify source-driven character profiles without changing the rig or animation."""
import base64
import copy
from io import BytesIO
import unittest

from PIL import Image
from endfield_native import character_profiles as subject


def png(color, size=(2, 2)):
    stream = BytesIO()
    Image.new('RGBA', size, color).save(stream, format='PNG')
    return stream.getvalue()


class NativeCharacterProfileTests(unittest.TestCase):
    def fixture(self, name='M_actor_fixture_hair_01'):
        source = {'source': 'material-cab', 'path_id': 1, 'name': name}
        slots = {'_SplitNormalMap': (128, 128, 100, 200),
                 '_DiffRampMap': (50, 100, 150, 200)}
        bindings, payloads = [], {}
        for i, (slot, color) in enumerate(slots.items(), 2):
            ref = {'source': 'texture-cab', 'path_id': i, 'name': slot}
            bindings.append({'slot': slot, 'reference': {'resolved': ref},
                'scale': [1, 1], 'offset': [0, 0], 'color_space': 0})
            payloads[('texture-cab', i)] = png(color, (4, 1) if slot == '_DiffRampMap' else (2, 2))
        row = {'object': source, 'textures': bindings,
            'floats': [{'name': '_UseDiffRampMap', 'value': 1},
                {'name': '_UseSpecBumpMap', 'value': 1}, {'name': '_EnableOutline', 'value': 1}],
            'colors': []}
        raw = [{'object': source, 'data': {'m_ValidKeywords': ['_DIFF_RAMP_ON', '_SPECULAR_NORMALMAP'],
            'disabledShaderPasses': []}}]
        doc = {'materials': [{'name': name}], 'nodes': [{'name': 'Head'}],
            'skins': [{'joints': [0]}], 'animations': [{'name': 'native-idle'}],
            'meshes': [{'primitives': [{'material': 0, 'attributes': {'POSITION': 2}}]}]}
        return doc, {'materials': [row]}, raw, payloads

    def test_hair_normal_data_and_linear_ramp_are_bound_with_explicit_semantics(self):
        doc, audit, raw, payloads = self.fixture()
        before = copy.deepcopy(doc)
        result, report = subject.restore_profiles(doc, audit, raw, payloads)
        profile = result['materials'][0]['extras']['azureRenderMaterial']
        self.assertEqual(profile['class'], 'hair')
        self.assertIn('hair-anisotropy', profile['features'])
        self.assertIn('diffuse-ramp', profile['features'])
        self.assertIn('silhouette-outline', profile['features'])
        data = result['images'][result['textures'][profile['hair']['dataTexture']]['source']]['uri']
        self.assertEqual(base64.b64decode(data.split(',', 1)[1]), payloads[('texture-cab', 2)])
        ramp = result['images'][result['textures'][profile['diffuseRamp']['texture']]['source']]['uri']
        self.assertEqual(base64.b64decode(ramp.split(',', 1)[1]), payloads[('texture-cab', 3)])
        for key in ('nodes', 'skins', 'animations', 'meshes'):
            self.assertEqual(result[key], before[key])
        self.assertEqual(doc, before)
        self.assertFalse(report['nativeVisualEquivalenceVerified'])

    def test_face_sdf_and_shadow_lut_survive_profile_completion(self):
        doc, audit, raw, payloads = self.fixture('M_actor_fixture_face_01')
        profile = {'schemaVersion': 2, 'class': 'face', 'features': ['face-sdf-eligible'],
            'faceSdf': {'schemaVersion': 2, 'texture': 41},
            'shadowColorLut': {'schemaVersion': 1, 'texture': 42}}
        doc['materials'][0]['extras'] = {'azureRenderMaterial': copy.deepcopy(profile)}
        result, _ = subject.restore_profiles(doc, audit, raw, payloads)
        current = result['materials'][0]['extras']['azureRenderMaterial']
        self.assertEqual(current['faceSdf'], profile['faceSdf'])
        self.assertEqual(current['shadowColorLut'], profile['shadowColorLut'])
        self.assertNotIn('diffuseRamp', current)

    def test_gray_overlay_mask_becomes_alpha_and_source_tint_becomes_color(self):
        doc, audit, raw, payloads = self.fixture('M_hairshadow_fixture')
        row = audit['materials'][0]
        row['floats'] = [{'name': '_UseGrayAsAlpha', 'value': 1}]
        row['colors'] = [{'name': '_BaseColor', 'rgba': [.25, .5, 1, .5]}]
        row['textures'] = [dict(row['textures'][0], slot='_BaseMap')]
        payloads[('texture-cab', 2)] = png((128, 255, 255, 255))
        result, _ = subject.restore_profiles(doc, audit, raw, payloads)
        material = result['materials'][0]
        index = material['pbrMetallicRoughness']['baseColorTexture']['index']
        uri = result['images'][result['textures'][index]['source']]['uri']
        pixel = Image.open(BytesIO(base64.b64decode(uri.split(',', 1)[1]))).getpixel((0, 0))
        self.assertEqual(pixel, (137, 188, 255, 64))
        self.assertEqual(material['alphaMode'], 'BLEND')
        self.assertEqual(material['extras']['azureRenderMaterial']['class'], 'overlay')

    def test_missing_active_texture_fails_and_disabled_outline_is_preserved(self):
        doc, audit, raw, payloads = self.fixture()
        with self.assertRaisesRegex(ValueError, 'Missing'):
            subject.restore_profiles(doc, audit, raw, {})
        raw[0]['data']['disabledShaderPasses'] = ['CharacterOutline']
        result, _ = subject.restore_profiles(doc, audit, raw, payloads)
        self.assertNotIn('silhouette-outline', result['materials'][0]['extras']['azureRenderMaterial']['features'])


if __name__ == '__main__':
    unittest.main()
