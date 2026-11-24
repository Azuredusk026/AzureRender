import copy
import unittest
from endfield_native.material import restore_opaque_surfaces


def source(**values):
    parameters = dict(_SurfaceType=0, _SrcBlend=1, _DstBlend=0, _AlphaClip=0, _EnableAlphaTest=0)
    parameters.update(values)
    return dict(object=dict(name='body', source='CAB-test', path_id=42),
                floats=[dict(name=k, value=v) for k,v in parameters.items()])


class MaterialTests(unittest.TestCase):
    def test_opaque_evidence_overrides_accidental_alpha_link(self):
        document = dict(materials=[dict(name='body', alphaMode='BLEND', pbrMetallicRoughness={'baseColorTexture': {'index': 3}})])
        result, report = restore_opaque_surfaces(document, {'materials': [source()]})
        self.assertEqual(result['materials'][0]['alphaMode'], 'OPAQUE')
        self.assertEqual(result['materials'][0]['pbrMetallicRoughness'], document['materials'][0]['pbrMetallicRoughness'])
        self.assertEqual(document['materials'][0]['alphaMode'], 'BLEND')
        self.assertEqual(len(report['restored']), 1)

    def test_transparency_and_clipping_are_not_guessed(self):
        for row in (source(_SurfaceType=1), source(_AlphaClip=1), source(_EnableAlphaTest=1), source(_SrcBlend=5)):
            document = dict(materials=[dict(name='body', alphaMode='BLEND')])
            result, report = restore_opaque_surfaces(document, {'materials': [row]})
            self.assertEqual(result, document)
            self.assertEqual(len(report['unresolved']), 1)

    def test_ambiguous_source_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Ambiguous'):
            restore_opaque_surfaces({'materials': [{'name':'body'}]}, {'materials':[source(),copy.deepcopy(source())]})


if __name__ == '__main__':
    unittest.main()
