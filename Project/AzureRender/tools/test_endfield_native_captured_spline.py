import copy
import struct
import unittest

try:
    from endfield_native.captured_spline import build_captured_emitter
except ImportError:
    build_captured_emitter = None


def fixture():
    blob = struct.pack('<9f', 2, 3, 4, 3, 3, 4, 2, 4, 4)
    document = {'asset': {'version': '2.0'}, 'buffers': [{'byteLength': len(blob)}],
                'bufferViews': [{'buffer': 0, 'byteOffset': 0, 'byteLength': len(blob)}],
                'accessors': [{'bufferView': 0, 'componentType': 5126, 'count': 3, 'type': 'VEC3'}],
                'materials': [{'name': 'opaque-cargo'}, {'name': 'inspection-part'}],
                'meshes': [{'primitives': [{'attributes': {'POSITION': 0}, 'material': 0},
                                            {'attributes': {'POSITION': 0}, 'material': 1}]}],
                'nodes': [{'mesh': 0}], 'scenes': [{'nodes': [0]}], 'scene': 0}
    slot = dict(times=[0, 1, 2], positions=[[5, 0, 1], [6, 2, 1], [5, 0, 1]],
                scales=[[2, 2, 2], [0, 0, 0], [2, 2, 2]], opacities=[1, 0, 1], sourceEuler=[0, 70, 0])
    clip = dict(format='NativeSplineRigidClip/1', identity=['fixture-source', 7], name='cargo-flow',
                period=2, slots=[slot], seed=42, sampleRate=1)
    world = [[1, 0, 0, 10], [0, 1, 0, 20], [0, 0, 1, 30], [0, 0, 0, 1]]
    bindings = [dict(primitive=1, origin=[2, 3, 4])]
    return document, blob, clip, world, bindings


def values(document, blob, accessor):
    row = document['accessors'][accessor];view = document['bufferViews'][row['bufferView']]
    width = {'SCALAR': 1, 'VEC3': 3}[row['type']]
    flat = struct.unpack_from('<'+'f'*(row['count']*width), blob, view.get('byteOffset', 0))
    return [list(flat[i:i+width]) for i in range(0, len(flat), width)]


class CapturedSplineTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(build_captured_emitter, 'captured geometry spline adapter missing')

    def test_preserves_capture_bytes_materials_and_template_geometry(self):
        document, blob, clip, world, bindings = fixture();before = copy.deepcopy(document)
        out, binary, report = build_captured_emitter(document, blob, clip, world, bindings)
        self.assertEqual(document, before)
        self.assertEqual(binary[:len(blob)], blob)
        self.assertEqual(out['materials'], document['materials'])
        self.assertEqual(out['meshes'][0]['primitives'][0], document['meshes'][0]['primitives'][1])
        self.assertEqual(report['templateTransformPolicy'], 'captured-orientation-and-size')
        self.assertFalse(report['nativeRandomVerified'])

    def test_moves_world_path_relative_to_captured_origin(self):
        document, blob, clip, world, bindings = fixture()
        out, binary, report = build_captured_emitter(document, blob, clip, world, bindings)
        animation = out['animations'][0]
        channel = next(c for c in animation['channels'] if c['target']['path'] == 'translation')
        self.assertEqual(values(out, binary, animation['samplers'][channel['sampler']]['output']),
                         [[13, 17, 27], [14, 19, 27], [13, 17, 27]])
        self.assertEqual(out['nodes'][0]['translation'], [13, 17, 27])

    def test_keeps_captured_size_and_explicit_inactive_gap(self):
        document, blob, clip, world, bindings = fixture()
        out, binary, report = build_captured_emitter(document, blob, clip, world, bindings)
        animation = out['animations'][0]
        channel = next(c for c in animation['channels'] if c['target']['path'] == 'scale')
        sampler = animation['samplers'][channel['sampler']]
        self.assertEqual(sampler['interpolation'], 'STEP')
        self.assertEqual(values(out, binary, sampler['output']), [[1, 1, 1], [0, 0, 0], [1, 1, 1]])
        self.assertEqual(out['nodes'][0]['scale'], [1, 1, 1])
        opacity = animation['extensions']['AZURE_node_opacity']['channels'][0]
        self.assertEqual(values(out, binary, animation['samplers'][opacity['sampler']]['output']), [[1], [0], [1]])

    def test_rotated_path_and_reused_templates_support_another_workflow(self):
        document, blob, clip, world, bindings = fixture()
        world = [[0, -1, 0, 1], [1, 0, 0, 2], [0, 0, 1, 3], [0, 0, 0, 1]]
        clip['slots'].append(copy.deepcopy(clip['slots'][0]))
        out, binary, report = build_captured_emitter(document, blob, clip, world, bindings)
        self.assertEqual(len(out['nodes']), 2)
        self.assertEqual(out['nodes'][0]['translation'], [-1, 4, 0])
        self.assertEqual(out['nodes'][1]['mesh'], 0)
        self.assertEqual(report['slotCount'], 2)

    def test_rejects_invalid_source_ownership_and_world_transform(self):
        for change in ['animated', 'missing-primitive', 'nan-origin', 'scaled-world']:
            document, blob, clip, world, bindings = fixture()
            if change == 'animated':document['animations'] = [{}]
            elif change == 'missing-primitive':bindings[0]['primitive'] = 22
            elif change == 'nan-origin':bindings[0]['origin'][0] = float('nan')
            else:world[0][0] = 2
            with self.subTest(change=change), self.assertRaises(ValueError):
                build_captured_emitter(document, blob, clip, world, bindings)

    def test_rejects_non_looping_and_invalid_curve_data(self):
        for change in ['loop', 'time', 'opacity']:
            document, blob, clip, world, bindings = fixture();slot = clip['slots'][0]
            if change == 'loop':slot['positions'][-1] = [99, 0, 0]
            elif change == 'time':slot['times'][1] = 0
            else:slot['opacities'][1] = 2
            with self.subTest(change=change), self.assertRaises(ValueError):
                build_captured_emitter(document, blob, clip, world, bindings)


if __name__ == '__main__':
    unittest.main()
