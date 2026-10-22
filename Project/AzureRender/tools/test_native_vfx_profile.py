"""Validate pinned VFX source selection independently of Blender."""
import copy
import unittest

from endfield_native.vfx_profile import unlit_vfx_profile


def source():
    return {
        'Name': 'IndependentVfxFixture',
        'Shader': {'Name': 'HGRP/Effect/VFXBaseV2',
                   'Source': 'CAB-9fe1ba4f3ce8edb2f54eb3e35709a336',
                   'PathId': -1430105248647086886},
        'Floats': {'_UseLighting': 0, '_UseForceDynamicLighting': 0,
                   '_EnvLightStrength': 0},
    }


class NativeVfxProfileTests(unittest.TestCase):
    def test_pinned_source_produces_explicit_unlit_surface(self):
        record = source()
        frozen = copy.deepcopy(record)
        self.assertEqual(unlit_vfx_profile(record), {
            'schemaVersion': 2, 'class': 'emissive',
            'features': ['emissive-mask', 'unlit-surface'],
            'surface': {'schemaVersion': 1, 'lighting': 'unlit'},
        })
        self.assertEqual(record, frozen)

    def test_source_selection_uses_identity(self):
        for key, value in [('Name', 'HGRP/Lit'), ('Source', 'other'),
                           ('PathId', 1), ('PathId', str(-1430105248647086886))]:
            with self.subTest(key=key, value=value):
                record = source()
                record['Shader'][key] = value
                with self.assertRaisesRegex(ValueError, 'identity'):
                    unlit_vfx_profile(record)

    def test_lighting_switches_are_required_and_zero(self):
        for key in source()['Floats']:
            for value in [None, True, '0', float('nan'), float('inf'), -1, .1, 1]:
                with self.subTest(key=key, value=value):
                    record = source()
                    if value is None:
                        record['Floats'].pop(key)
                    else:
                        record['Floats'][key] = value
                    with self.assertRaisesRegex(ValueError, key):
                        unlit_vfx_profile(record)

    def test_fresh_result_has_independent_ownership(self):
        first = unlit_vfx_profile(source())
        first['features'].clear()
        first['surface']['lighting'] = 'other'
        self.assertEqual(unlit_vfx_profile(source())['surface']['lighting'], 'unlit')
        self.assertEqual(len(unlit_vfx_profile(source())['features']), 2)


if __name__ == '__main__':
    unittest.main()
