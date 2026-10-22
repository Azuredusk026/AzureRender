"""Public source-channel fixtures for character surface exchange."""
import copy
import base64
from io import BytesIO
import unittest
from PIL import Image
from endfield_native import character_surfaces as subject


class CharacterSurfaceTests(unittest.TestCase):
    def test_explicit_specular_preserves_each_source_channel(self):
        png=BytesIO();Image.new('RGBA',(1,1),(51,76,102,204)).save(png,format='PNG')
        obj={'source':'cab','path_id':1,'name':'cloth'}
        tex={'source':'tex','path_id':2,'name':'parameter'}
        row={'object':obj,'shader':{'resolved':{}},'colors':[],
            'floats':[{'name':'_UseMetallicGlossMap','value':1}],
            'textures':[{'slot':'_MetallicGlossMap','reference':{'resolved':tex},
                         'scale':[1,1],'offset':[0,0],'color_space':0}]}
        doc={'materials':[{'name':'cloth','extras':{'azureRenderMaterial':{
            'schemaVersion':2,'class':'fabric','features':[]}}}]}
        args=(doc,{'materials':[row]},[{'object':obj,'data':{}}],{('tex',2):png.getvalue()})
        result,report=subject.restore_surfaces(*args,specular_materials=['cloth'])
        payload=base64.b64decode(result['images'][0]['uri'].split(',')[1])
        self.assertEqual(Image.open(BytesIO(payload)).getpixel((0,0)),(102,51,51,76))
        self.assertEqual(result['materials'][0]['extras']['azureRenderMaterial']['specularInput'],
            {'schemaVersion':1,'source':'metallic-roughness-alpha','scale':.5})
        self.assertNotIn('source G specular',report['materials'][0]['bound'][0]['unconsumed_channels'])
        doc['materials'][0]['emissiveTexture']={'index':97}
        doc['materials'].append({'name':'other','extras':{'unrelated':1}})
        restricted,_=subject.restore_surfaces(*args,specular_materials=['cloth'],specular_only=True)
        self.assertEqual(restricted['materials'][0]['emissiveTexture'],{'index':97})
        self.assertEqual(restricted['materials'][1],doc['materials'][1])
        with self.assertRaises(ValueError):subject.restore_surfaces(*args,specular_materials=['missing'])
        row['floats'][0]['value']=0
        with self.assertRaises(ValueError):subject.restore_surfaces(*args,specular_materials=['cloth'])

    def test_unity_normal_and_hair_split_have_distinct_strength_order(self):
        pixels = Image.new('RGBA', (1, 1), (191, 128, 0, 255))
        unity = subject.normal_image(pixels, 0.5, 'unity-rag')
        split = subject.normal_image(pixels, 0.5, 'hair-rg')
        self.assertLess(unity.getpixel((0, 0))[2], split.getpixel((0, 0))[2])
        self.assertGreater(unity.getpixel((0, 0))[0], split.getpixel((0, 0))[0])
        self.assertGreater(unity.getpixel((0, 0))[2], 230)
        alpha_packed = Image.new('RGBA', (1, 1), (255, 128, 0, 128))
        self.assertEqual(subject.normal_image(alpha_packed, 1, 'unity-rag').getpixel((0, 0)), (128, 128, 255, 255))

    def test_msao_smoothness_repacked_without_srgb_conversion(self):
        packed = Image.new('RGBA', (1, 1), (51, 76, 102, 204))
        self.assertEqual(subject.orm_image(packed).getpixel((0, 0)), (102, 51, 51, 255))

    def test_linear_emission_pixels_encode_for_standard_gltf(self):
        linear = Image.new('RGBA', (1, 1), (26, 51, 102, 77))
        self.assertEqual(subject.color_image(linear, 0).getpixel((0, 0)), (90, 124, 170, 77))
        self.assertEqual(subject.color_image(linear, 1).getpixel((0, 0)), linear.getpixel((0, 0)))

    def test_hdr_emission_uses_source_tint_and_brightness(self):
        png = BytesIO();Image.new('RGBA', (1, 1), (255, 255, 255, 255)).save(png, format='PNG')
        obj = {'source': 'cab', 'path_id': 1, 'name': 'emission'}
        texture = {'source': 'tex', 'path_id': 2, 'name': 'source-emission'}
        row = {'object': obj, 'shader': {'resolved': {'source': 'shader', 'path_id': 3}},
               'textures': [{'slot': '_EmissionMap', 'reference': {'resolved': texture},
                             'scale': [1, 1], 'offset': [0, 0], 'color_space': 1}],
               'floats': [{'name': '_EmissionBrightness', 'value': 4}],
               'colors': [{'name': '_EmissionColor', 'rgba': [.5, .25, .125, 1]}]}
        result, _ = subject.restore_surfaces({'materials': [{'name': 'emission'}]}, {'materials': [row]},
            [{'object': obj, 'data': {'m_ValidKeywords': ['_EMISSION']}}], {('tex', 2): png.getvalue()})
        material = result['materials'][0]
        self.assertEqual(material['emissiveFactor'], [1, .5, .25])
        self.assertEqual(material['extensions']['KHR_materials_emissive_strength']['emissiveStrength'], 2)

    def test_disabled_maps_do_not_activate_and_structure_is_preserved(self):
        doc = {'materials': [{'name': 'fixture', 'alphaMode': 'MASK', 'alphaCutoff': .7}],
               'nodes': [{'name': 'Head'}], 'skins': [{'joints': [0]}],
               'meshes': [{'primitives': [{'material': 0, 'attributes': {'POSITION': 1}}]}],
               'animations': [{'name': 'idle', 'samplers': [{'input': 2, 'output': 3}]}]}
        audit = {'materials': [{'object': {'source': 'cab', 'path_id': 1, 'name': 'fixture'},
                 'shader': {'resolved': {'source': 'shader', 'path_id': 2}},
                 'textures': [], 'floats': [{'name': '_Metallic', 'value': .3},
                     {'name': '_Smoothness', 'value': .8}], 'colors': []}]}
        sources = [{'object': audit['materials'][0]['object'], 'data': {'m_ValidKeywords': []}}]
        before = copy.deepcopy(doc)
        result, report = subject.restore_surfaces(doc, audit, sources, {})
        self.assertEqual(doc, before)
        for key in ('nodes', 'skins', 'meshes', 'animations'):
            self.assertEqual(result[key], before[key])
        self.assertEqual(result['materials'][0]['alphaMode'], 'MASK')
        self.assertEqual(result['materials'][0]['alphaCutoff'], .7)
        self.assertNotIn('normalTexture', result['materials'][0])
        self.assertNotIn('emissiveTexture', result['materials'][0])
        self.assertAlmostEqual(result['materials'][0]['pbrMetallicRoughness']['roughnessFactor'], .2)
        self.assertFalse(report['shader_equivalence_verified'])

    def test_source_maps_enter_standard_gltf_and_missing_source_fails(self):
        png = BytesIO();Image.new('RGBA', (1, 1), (128, 128, 50, 255)).save(png, format='PNG')
        obj = {'source': 'cab', 'path_id': 1, 'name': 'fixture'}
        texture = {'source': 'tex', 'path_id': 2, 'name': 'normal'}
        doc = {'materials': [{'name': 'fixture'}]}
        row = {'object': obj, 'shader': {'resolved': {'source': 'shader', 'path_id': 3}},
               'textures': [{'slot': '_BumpMap', 'reference': {'resolved': texture},
                             'scale': [1, 1], 'offset': [0, 0], 'color_space': 0}],
               'floats': [{'name': '_UseBumpMap', 'value': 1}, {'name': '_BumpScale', 'value': 1},
                          {'name': '_Metallic', 'value': 0}, {'name': '_Smoothness', 'value': .2}], 'colors': []}
        sources = [{'object': obj, 'data': {'m_ValidKeywords': ['_NORMALMAP']}}]
        with self.assertRaises(ValueError): subject.restore_surfaces(doc, {'materials': [row]}, sources, {})
        result, _ = subject.restore_surfaces(doc, {'materials': [row]}, sources, {('tex', 2): png.getvalue()})
        self.assertEqual(result['materials'][0]['normalTexture']['index'], 0)
        self.assertNotIn('azureRenderMaterial', result['materials'][0].get('extras', {}))


if __name__ == '__main__': unittest.main()
