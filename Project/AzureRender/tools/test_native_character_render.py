import copy
import unittest
from endfield_native.character_layers import restore_layers


class CharacterLayersTests(unittest.TestCase):
    def fixture(self):
        identity = {'source': 'cab', 'path_id': 4, 'name': 'overlay'}
        renderer = {'path': 'root/hair', 'enabled': True, 'submesh_count': 1,
                    'materials': [{'resolved': {'source': 'cab', 'path_id': 3}}, {'resolved': identity}]}
        source = {'object': identity, 'data': {'disabledShaderPasses': [],
                  'm_SavedProperties': {'m_Floats': [{'Key': k, 'Value': v} for k, v in
                      {'_SurfaceType': 1, '_SrcBlend': 5, '_DstBlend': 10, '_AlphaClip': 0}.items()]}}}
        audit = {'renderers': [renderer], 'transforms': [
            {'path': 'root', 'game_object_active': True}, {'path': 'root/hair', 'game_object_active': True}]}
        doc = {'meshes': [{'name': 'hair', 'primitives': [{'attributes': {'POSITION': 0}, 'indices': 1, 'material': 0}]}],
               'materials': [{'name': 'base'}], 'nodes': [{'name': 'hair', 'mesh': 0, 'skin': 0}],
               'skins': [{'joints': [1]}], 'animations': [{'name': 'idle'}]}
        return doc, audit, [source]

    def test_append_preserves_skin_animation_and_input(self):
        doc, audit, sources = self.fixture(); before = copy.deepcopy(doc)
        result, report = restore_layers(doc, audit, sources, {})
        self.assertEqual(doc, before)
        self.assertEqual(result['animations'], doc['animations'])
        self.assertEqual(result['skins'], doc['skins'])
        self.assertEqual(result['meshes'][0]['primitives'][1]['indices'], 1)
        self.assertEqual(result['materials'][1]['alphaMode'], 'BLEND')
        self.assertFalse(report['shader_equivalence_verified'])

    def test_disabled_forward_retained_as_unresolved(self):
        doc, audit, sources = self.fixture()
        sources[0]['data']['disabledShaderPasses'] = ['ForwardOnly']
        result, report = restore_layers(doc, audit, sources, {})
        self.assertEqual(len(result['meshes'][0]['primitives']), 1)
        self.assertEqual(report['unresolved'][0]['reason'], 'source_forward_pass_disabled')

    def test_inactive_ancestor_not_drawn(self):
        doc, audit, sources = self.fixture(); audit['transforms'][0]['game_object_active'] = False
        result, report = restore_layers(doc, audit, sources, {})
        self.assertEqual(len(result['meshes'][0]['primitives']), 1)

    def test_ambiguous_renderer_rejected(self):
        doc, audit, sources = self.fixture(); audit['renderers'].append(copy.deepcopy(audit['renderers'][0]))
        with self.assertRaises(ValueError): restore_layers(doc, audit, sources, {})

    def test_source_texture_and_uv_are_preserved(self):
        doc, audit, sources = self.fixture()
        tex = {'source': 'texture-cab', 'path_id': 8, 'name': 'source-color'}
        audit['materials'] = [{'object': sources[0]['object'], 'textures': [
            {'slot': '_BaseMap', 'reference': {'resolved': tex}, 'scale': [2, 1], 'offset': [0.5, 0]}]}]
        with self.assertRaises(ValueError): restore_layers(doc, audit, sources, {})
        result, _ = restore_layers(doc, audit, sources, {('texture-cab', 8): b'png-fixture'})
        import base64
        self.assertEqual(base64.b64decode(result['images'][0]['uri'].split(',')[1]), b'png-fixture')
        info = result['materials'][1]['pbrMetallicRoughness']['baseColorTexture']
        self.assertEqual(info['extensions']['KHR_texture_transform']['scale'], [2, 1])


if __name__ == '__main__': unittest.main()
