"""Container pointers bind meshes even when importer labels are stale."""
import unittest

try:
    from endfield_native.mesh_binding import resolve_mesh_binding
except ImportError:
    resolve_mesh_binding = None


class MeshBindingTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(callable(resolve_mesh_binding), 'mesh pointer resolver required')
        self.asset = dict(Source='CAB-one', PathId=-17, Type='Mesh', Name='actual_mesh')
        self.containers = {'models/mesh.fbx': self.asset}
        self.exports = [dict(Asset=dict(self.asset), File='meshes/mesh.obj')]

    def test_exact_container_pointer_resolves_stale_subasset_label(self):
        asset, export, alias = resolve_mesh_binding('models/mesh.fbx##import_label',
                                                  self.containers, self.exports)
        self.assertEqual((asset['Source'], asset['PathId']), ('CAB-one', -17))
        self.assertEqual(export['File'], 'meshes/mesh.obj')
        self.assertEqual(alias, dict(requested='import_label', bound='actual_mesh'))

    def test_matching_label_keeps_exact_identity(self):
        _, export, alias = resolve_mesh_binding('MODELS/mesh.fbx##actual_mesh',
                                               self.containers, self.exports)
        self.assertEqual(export['Asset']['PathId'], -17)
        self.assertIsNone(alias)

    def test_same_name_in_other_serialized_file_cannot_replace_pointer(self):
        self.exports[0]['Asset']['Source'] = 'CAB-other'
        with self.assertRaisesRegex(ValueError, 'export'):
            resolve_mesh_binding('models/mesh.fbx##actual_mesh', self.containers, self.exports)

    def test_non_mesh_container_and_duplicate_pointer_export_are_rejected(self):
        self.containers['models/mesh.fbx'] = dict(self.asset, Type='GameObject')
        with self.assertRaises(ValueError):
            resolve_mesh_binding('models/mesh.fbx', self.containers, self.exports)
        self.containers['models/mesh.fbx'] = self.asset
        with self.assertRaisesRegex(ValueError, 'export'):
            resolve_mesh_binding('models/mesh.fbx', self.containers, self.exports * 2)

    def test_unexported_mesh_and_unsupported_suffix_are_rejected(self):
        for ref in ['models/mesh.fbx##', 'models/mesh.fbx##a##b', 'missing.fbx']:
            with self.subTest(ref=ref), self.assertRaises(ValueError):
                resolve_mesh_binding(ref, self.containers, self.exports)
        self.exports[0].pop('File')
        with self.assertRaisesRegex(ValueError, 'export'):
            resolve_mesh_binding('models/mesh.fbx', self.containers, self.exports)


if __name__ == '__main__':unittest.main()
