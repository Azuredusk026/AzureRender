import copy
import unittest
import struct
import character_intake as intake

class ResourceValidationTests(unittest.TestCase):
    def test_missing_optional_clip_does_not_require_retarget(self):
        self.assertTrue(hasattr(intake, 'retarget_required'))
        self.assertFalse(intake.retarget_required({'asset': {'skeletonFingerprint': 'a'}, 'idle': {'skeletonFingerprint': 'a'}, 'walk': {'status': 'missing'}}))

    def test_inverse_bind_values_participate_in_fingerprint(self):
        document = dict(nodes=[dict(name='root')], skins=[dict(joints=[0], inverseBindMatrices=0)], accessors=[dict(count=1, type='MAT4', componentType=5126, bufferView=0)], bufferViews=[dict(buffer=0, byteLength=64)])
        first = [1.0 if i % 5 == 0 else 0.0 for i in range(16)]
        second = first.copy()
        second[12] = .5
        self.assertNotEqual(intake.audit_gltf(document, [struct.pack('<16f', *first)])['skeletonFingerprint'], intake.audit_gltf(document, [struct.pack('<16f', *second)])['skeletonFingerprint'])

    def manifest(self):
        return dict(schemaVersion=1, sources=[dict(id='model', sha256='a'*64, dependencies=[])], tools=[], settings={}, outputs=[], counts={}, losses=[], evidence=[], sourceKind='procedural')

    def test_manifest_rejects_missing_hash_version_and_dependency(self):
        self.assertTrue(hasattr(intake, 'validate_manifest'), 'manifest validation is required')
        intake.validate_manifest(self.manifest())
        for change, message in [('hash', 'sha256'), ('version', 'version'), ('dependency', 'dependency')]:
            value = self.manifest()
            if change == 'hash': del value['sources'][0]['sha256']
            if change == 'version': value['schemaVersion'] = 2
            if change == 'dependency': value['sources'][0]['dependencies'] = ['missing']
            with self.assertRaisesRegex(ValueError, message): intake.validate_manifest(value)

    def test_statistics_and_hierarchy_fingerprint(self):
        document = dict(nodes=[dict(name='root', children=[1]), dict(name='face')], skins=[dict(joints=[0,1])], accessors=[dict(count=3, sparse={})], meshes=[dict(primitives=[dict(attributes=dict(POSITION=0, JOINTS_1=0, WEIGHTS_1=0), targets=[{}, {}, {}])])])
        result = intake.audit_gltf(document)
        self.assertEqual(result.get('skinCount'), 1)
        self.assertEqual(result['morphTargetCounts'], [3])
        self.assertEqual(result['sparseAccessorCount'], 1)
        self.assertEqual(result['secondaryJointPrimitiveCount'], 1)
        changed = copy.deepcopy(document)
        changed['nodes'][0].pop('children')
        self.assertNotEqual(result['skeletonFingerprint'], intake.audit_gltf(changed)['skeletonFingerprint'])
        changed = copy.deepcopy(document)
        changed['nodes'][1]['translation'] = [0,1,0]
        self.assertNotEqual(result['skeletonFingerprint'], intake.audit_gltf(changed)['skeletonFingerprint'])

if __name__ == '__main__': unittest.main()
