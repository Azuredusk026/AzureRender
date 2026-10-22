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
    def projected_fixture(self):
        doc, audit, raw, payloads = self.fixture()
        row = audit['materials'][0]
        row['shader'] = {'resolved': {'source': 'CAB-623a5cbbe298e3f04194d1e14a05ebfe',
            'path_id': -8095970123935614097}}
        parameters = {'_UseSpecRampMap': 1, '_AnisotropyValue': .49,
            '_AnisotropyValue2': .51, '_AnisotropyIntensity': 1.3,
            '_AnisotropyRange2': .5, '_AnisotropyEdgeFade': 5,
            '_SpecBumpScale': 1, '_BumpScale': .7, '_AnisotropyDirX': 0, '_BackFaceNormalFlip': 0}
        row['floats'] += [{'name': key, 'value': value} for key, value in parameters.items()]
        row['colors'].append({'name': '_AnisotropyColor2', 'rgba': [.672, 0, .004363678, 1]})
        raw[0]['data']['m_ValidKeywords'].append('_SPEC_RAMP_ON')
        for index, slot in ((4, '_SpecRampMap'), (5, '_MetallicGlossMap')):
            row['textures'].append({'slot': slot, 'reference': {'resolved': {
                'source': 'texture-cab', 'path_id': index, 'name': slot}},
                'scale': [1, 1], 'offset': [0, 0], 'color_space': 0})
            stream = BytesIO()
            image = Image.new('RGBA', (2, 2))
            image.putdata([(1, 2, 3, 4), (5, 6, 7, 8), (11, 12, 13, 14), (15, 16, 17, 18)])
            image.save(stream, format='PNG')
            payloads[('texture-cab', index)] = stream.getvalue()
        return doc, audit, raw, payloads

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

    def test_pinned_overlay_shader_enables_typed_multiply_and_records_native_gaps(self):
        doc, audit, raw, payloads = self.fixture('M_hairshadow_fixture')
        row = audit['materials'][0]
        row['floats'] = [{'name': '_UseGrayAsAlpha', 'value': 1}]
        row['colors'] = [{'name': '_BaseColor', 'rgba': [.25, .5, 1, .5]}]
        row['textures'] = [dict(row['textures'][0], slot='_BaseMap')]
        row['shader'] = {'resolved': {'source': 'CAB-da44c449ed4aff035ab44ec6c74bdbfe',
            'path_id': 1962491951764993823, 'type': 'Shader', 'name': ''}}
        payloads[('texture-cab', 2)] = png((128, 255, 255, 255))
        result, report = subject.restore_profiles(doc, audit, raw, payloads)
        profile = result['materials'][0]['extras']['azureRenderMaterial']
        self.assertEqual(profile['composite'], {'schemaVersion': 1, 'mode': 'multiply'})
        self.assertEqual(profile['features'], ['multiply-composite', 'overlay'])
        self.assertEqual(report['materials'][0]['compositeSource']['colorBlend'],
            {'source': 'Zero', 'destination': 'SrcColor'})
        self.assertIn('stencil', report['materials'][0]['remainingNativeCompositeInputs'])
        self.assertFalse(report['nativeVisualEquivalenceVerified'])
        for key in ('nodes', 'skins', 'animations', 'meshes'):
            self.assertEqual(result[key], doc[key])
        row['shader']['resolved']['path_id'] += 1
        other, other_report = subject.restore_profiles(result, audit, raw, payloads)
        self.assertNotIn('composite', other['materials'][0]['extras']['azureRenderMaterial'])
        self.assertNotIn('multiply-composite', other['materials'][0]['extras']['azureRenderMaterial']['features'])
        self.assertIn('unverified-overlay-shader', other_report['materials'][0]['remainingNativeCompositeInputs'])

    def test_pinned_hair_binds_original_two_dimensional_ramp_and_parameter_channels(self):
        doc, audit, raw, payloads = self.projected_fixture()
        doc['materials'][0]['extras'] = {'azureRenderMaterial': {'schemaVersion': 2,
            'class': 'hair', 'features': ['hair-ramp', 'face-sdf-eligible'],
            'faceSdf': {'texture': 42}, 'hair': {'rampTexture': 41}}}
        output, report = subject.restore_profiles(doc, audit, raw, payloads)
        profile = output['materials'][0]['extras']['azureRenderMaterial']
        self.assertIn('hair-projected-ramp', profile['features'])
        self.assertIn('hair-anisotropy', profile['features'])
        self.assertNotIn('hair-ramp', profile['features'])
        self.assertNotIn('faceSdf', profile)
        self.assertNotIn('rampTexture', profile['hair'])
        projected = profile['hair']['projectedRamp']
        for key, expected in {'schemaVersion': 1, 'primaryShift': -.02, 'secondaryShift': .02,
            'intensity': 1.3, 'secondaryRange': .5, 'edgeFade': 5,
            'specularNormalScale': 1, 'surfaceNormalScale': .7, 'directionX': 0}.items():
            self.assertAlmostEqual(projected[key], expected)
        self.assertEqual(projected['secondaryColor'], [.672, 0, .004363678])
        self.assertEqual(projected['objectBasis'], [1, 0, 0, 0, 1, 0, 0, 0, 1])
        for key, source_index in (('rampTexture', 4), ('parameterTexture', 5)):
            uri = output['images'][output['textures'][projected[key]]['source']]['uri']
            self.assertEqual(base64.b64decode(uri.split(',', 1)[1]), payloads[('texture-cab', source_index)])
        for key in ('nodes', 'skins', 'animations', 'meshes'):
            self.assertEqual(output[key], doc[key])
        self.assertFalse(report['nativeVisualEquivalenceVerified'])
        self.assertIn('main-virtual-light-global-bindings', report['materials'][0]['remainingProjectedHairInputs'])

    def test_projected_hair_requires_pinned_shader_complete_source_and_linear_maps(self):
        doc, audit, raw, payloads = self.projected_fixture()
        audit['materials'][0]['floats'] = [value for value in audit['materials'][0]['floats']
            if value['name'] != '_AnisotropyValue2']
        with self.assertRaisesRegex(ValueError, 'Missing active projected hair field'):
            subject.restore_profiles(doc, audit, raw, payloads)
        for slot in ('_SpecRampMap', '_MetallicGlossMap'):
            doc, audit, raw, payloads = self.projected_fixture()
            next(binding for binding in audit['materials'][0]['textures'] if binding['slot'] == slot)['color_space'] = 1
            with self.subTest(slot=slot), self.assertRaisesRegex(ValueError, 'linear'):
                subject.restore_profiles(doc, audit, raw, payloads)
        doc, audit, raw, payloads = self.projected_fixture()
        output, _ = subject.restore_profiles(doc, audit, raw, payloads)
        audit['materials'][0]['shader']['resolved']['path_id'] += 1
        other, _ = subject.restore_profiles(output, audit, raw, payloads)
        profile = other['materials'][0]['extras']['azureRenderMaterial']
        self.assertNotIn('hair-projected-ramp', profile['features'])
        self.assertNotIn('projectedRamp', profile['hair'])

    def test_pinned_hair_keeps_strand_parameters_out_of_metallic_roughness_and_ao(self):
        doc, audit, raw, payloads = self.projected_fixture()
        material = doc['materials'][0]
        material['pbrMetallicRoughness'] = {'metallicRoughnessTexture': {'index': 42},
            'metallicFactor': 1, 'roughnessFactor': .2, 'baseColorFactor': [.2, .3, .4, 1]}
        material['extras'] = {'azureRenderMaterial': {'schemaVersion': 2, 'class': 'hair',
            'features': ['surface-ao']}}
        output, report = subject.restore_profiles(doc, audit, raw, payloads)
        current = output['materials'][0]
        self.assertNotIn('metallicRoughnessTexture', current['pbrMetallicRoughness'])
        self.assertEqual(current['pbrMetallicRoughness']['metallicFactor'], 0)
        self.assertEqual(current['pbrMetallicRoughness']['roughnessFactor'], 1)
        self.assertEqual(current['pbrMetallicRoughness']['baseColorFactor'], [.2, .3, .4, 1])
        profile = current['extras']['azureRenderMaterial']
        self.assertNotIn('surface-ao', profile['features'])
        self.assertEqual(profile['hair']['projectedRamp']['surfaceResponse'], 'strand-diffuse')
        self.assertEqual(profile['hair']['projectedRamp']['diffuseScale'], .96)
        surface = report['materials'][0]['projectedHairSource']['surfaceSemantics']
        self.assertEqual(surface['roughnessStatus'], 'host-compatibility-placeholder')
        self.assertIn('parameter-B-shadow-ramp-screen-rim',
            report['materials'][0]['remainingProjectedHairInputs'])
        audit['materials'][0]['shader']['resolved']['path_id'] += 1
        unverified, _ = subject.restore_profiles(doc, audit, raw, payloads)
        self.assertEqual(unverified['materials'][0]['pbrMetallicRoughness'], material['pbrMetallicRoughness'])

    def test_projected_hair_rejects_nonfinite_ranges_and_collapsed_ramp(self):
        for field, value in (('_AnisotropyValue', 2), ('_AnisotropyIntensity', float('nan')),
                             ('_BumpScale', 9), ('_AnisotropyRange2', -1)):
            doc, audit, raw, payloads = self.projected_fixture()
            next(item for item in audit['materials'][0]['floats'] if item['name'] == field)['value'] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'projected hair|Projected hair'):
                subject.restore_profiles(doc, audit, raw, payloads)
        doc, audit, raw, payloads = self.projected_fixture()
        payloads[('texture-cab', 4)] = png((13, 59, 101, 197), (4, 1))
        with self.assertRaisesRegex(ValueError, 'two dimensions'):
            subject.restore_profiles(doc, audit, raw, payloads)
        doc, audit, raw, payloads = self.projected_fixture()
        next(item for item in audit['materials'][0]['floats'] if item['name'] == '_BackFaceNormalFlip')['value'] = 1
        with self.assertRaisesRegex(ValueError, 'back-face normal'):
            subject.restore_profiles(doc, audit, raw, payloads)

    def test_pinned_overlay_cull_back_uses_shader_pass_provenance(self):
        for name in ('M_hairshadow_fixture', 'M_eyewhiteshadow_fixture'):
            doc, audit, raw, payloads = self.fixture(name)
            doc['materials'][0]['doubleSided'] = True
            row = audit['materials'][0]
            row['floats'] = [{'name': '_UseGrayAsAlpha', 'value': 1}]
            row['colors'] = [{'name': '_BaseColor', 'rgba': [1, 1, 1, 1]}]
            row['textures'] = [dict(row['textures'][0], slot='_BaseMap')]
            row['shader'] = {'resolved': {'source': 'CAB-da44c449ed4aff035ab44ec6c74bdbfe',
                'path_id': 1962491951764993823}}
            output, report = subject.restore_profiles(doc, audit, raw, payloads)
            self.assertFalse(output['materials'][0]['doubleSided'])
            provenance = report['materials'][0]['cullingSource']
            self.assertEqual(provenance['pass'], 1)
            self.assertEqual(provenance['programs'], [10, 11])
            self.assertEqual(provenance['culling'], {'value': 2, 'mode': 'Back'})
            row['shader']['resolved']['path_id'] += 1
            other, other_report = subject.restore_profiles(doc, audit, raw, payloads)
            self.assertTrue(other['materials'][0]['doubleSided'])
            self.assertNotIn('cullingSource', other_report['materials'][0])


if __name__ == '__main__':
    unittest.main()
