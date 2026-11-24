import struct
import unittest

import numpy as np

from convert_eye_vertex_basis import convert_eye_basis
from endfield_native.character_tangents import accessor


class EyeVertexBasisTests(unittest.TestCase):
    def setUp(self):
        # Interleaved UV/tangent data is shared with a non-eye primitive.
        rows = np.array([[.25, .125, 1, 0, 0, 1], [.75, .875, 0, 1, 0, -1]], dtype='<f4')
        raw = rows.tobytes()
        self.tail = struct.pack('<II', len(raw), 0x004e4942) + raw
        self.model = {
            'buffers': [{'byteLength': len(raw)}],
            'bufferViews': [{'buffer': 0, 'byteLength': len(raw), 'byteStride': 24}],
            'accessors': [{'bufferView': 0, 'type': kind, 'componentType': 5126,
                           'byteOffset': offset, 'count': 2} for kind, offset in [('VEC2', 0), ('VEC4', 8)]],
            'materials': [{'name': 'iris', 'extras': {'azureRenderMaterial': {'class': 'eye'}}},
                          {'name': 'cloth'}],
            'meshes': [{'primitives': [
                {'material': i, 'attributes': {'TEXCOORD_0': 0, 'TANGENT': 1}} for i in [0, 1, 0]]}]
        }

    def test_conversion_preserves_shared_geometry(self):
        out, binary = convert_eye_basis(self.model, self.tail, 'name:iris')
        first, other, second = out['meshes'][0]['primitives']
        self.assertEqual(first['attributes'], second['attributes'])
        self.assertEqual(other, self.model['meshes'][0]['primitives'][1])
        self.assertEqual(self.tail[8:], binary[8:len(self.tail)])
        np.testing.assert_array_equal(accessor(out, binary, first['attributes']['TEXCOORD_0']),
                                      [[.25, .875], [.75, .125]])
        np.testing.assert_array_equal(accessor(out, binary, first['attributes']['TANGENT']),
                                      [[1, 0, 0, -1], [0, 1, 0, 1]])
        with self.assertRaisesRegex(ValueError, 'already declares'):
            convert_eye_basis(out, binary, 'name:iris')

    def test_invalid_and_unselected_inputs(self):
        for slot in ['name:cloth', 'unknown']:
            with self.assertRaises(ValueError):
                convert_eye_basis(self.model, self.tail, slot)
        with self.assertRaisesRegex(ValueError, 'binary chunk'):
            convert_eye_basis(self.model, self.tail[:-1], 'name:iris')
        del self.model['meshes'][0]['primitives'][0]['attributes']['TANGENT']
        with self.assertRaisesRegex(ValueError, 'missing TANGENT'):
            convert_eye_basis(self.model, self.tail, 'name:iris')


if __name__ == '__main__':
    unittest.main()
