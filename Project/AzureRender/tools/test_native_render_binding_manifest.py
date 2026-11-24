"""Evidence must reject missing draws and identities from another capture."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

try:
    from native_render.binding_manifest import build_manifest
except ImportError:
    build_manifest = None


class BindingManifestTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(callable(build_manifest), 'Capture binding manifest builder is required')
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.capture = self.root / 'source.rdc'
        self.capture.write_bytes(b'capture-a')
        self.inventory = dict(schema_version=2, mode='resources',
            capture_path=str(self.capture), api='GraphicsAPI.D3D11', complete=True, errors=[],
            actions=[dict(eid=7, kind='Draw'), dict(eid=19, kind='Dispatch')],
            resources=[dict(resourceId='ResourceId::42', name='texture')],
            textures=[dict(resourceId='ResourceId::42', width=16, height=8)], buffers=[])
        binding = dict(location=dict(slot=8, reflection_index=0, array_element=0),
            descriptor=dict(resource='ResourceId::42', firstMip=0, numMips=1))
        self.pages = [dict(schema_version=2, mode='pipelines', capture_path=str(self.capture),
            api='GraphicsAPI.D3D11', complete=True, errors=[], after_eid=0,
            next_after_eid=7, has_more=True, pipelines=[dict(eid=7, kind='Draw', errors=[],
                stages={'PS':dict(bytecode_sha256='a'*64, bindings={'srv':[binding]})})]),
            dict(schema_version=2, mode='pipelines', capture_path=str(self.capture),
            api='GraphicsAPI.D3D11', complete=True, errors=[], after_eid=7,
            next_after_eid=19, has_more=False, pipelines=[dict(eid=19, kind='Dispatch',
                errors=[], stages={'CS':dict(bytecode_sha256='b'*64, bindings={})})])]

    def build(self):
        inventory = self.root / 'inventory.json'
        inventory.write_text(json.dumps(self.inventory))
        pages = []
        for i, page in enumerate(self.pages):
            path = self.root / ('page%d.json' % i)
            path.write_text(json.dumps(page)); pages.append(path)
        return build_manifest(self.capture, inventory, pages)

    def test_preserves_hardware_slot_separately_from_reflection_index(self):
        result = self.build()
        self.assertEqual(result['capture']['sha256'], hashlib.sha256(b'capture-a').hexdigest())
        binding = result['events'][0]['stages']['PS']['bindings']['srv'][0]
        self.assertEqual(binding['location']['slot'], 8)
        self.assertEqual(binding['location']['reflection_index'], 0)
        self.assertEqual([x['eid'] for x in result['events']], [7, 19])
        self.assertFalse(result['coverage']['raw_payloads_exported'])

    def test_rejects_missing_final_draw(self):
        self.pages = self.pages[:1]
        self.pages[0]['has_more'] = False
        with self.assertRaisesRegex(ValueError, 'coverage'):
            self.build()

    def test_rejects_another_capture_even_with_same_event_ids(self):
        self.pages[1]['capture_path'] = str(self.root / 'other.rdc')
        with self.assertRaisesRegex(ValueError, 'capture'):
            self.build()

    def test_rejects_page_gap_and_duplicate_event(self):
        self.pages[1]['after_eid'] = 8
        with self.assertRaisesRegex(ValueError, 'page'):
            self.build()
        self.pages[1]['after_eid'] = 7
        self.pages[1]['pipelines'].insert(0, copy.deepcopy(self.pages[0]['pipelines'][0]))
        with self.assertRaisesRegex(ValueError, 'event'):
            self.build()

    def test_rejects_unknown_binding_and_unresolved_slot(self):
        binding = self.pages[0]['pipelines'][0]['stages']['PS']['bindings']['srv'][0]
        binding['descriptor']['resource'] = 'ResourceId::999'
        with self.assertRaisesRegex(ValueError, 'resource'):
            self.build()
        binding['descriptor']['resource'] = 'ResourceId::42'
        binding['location']['slot'] = None
        with self.assertRaisesRegex(ValueError, 'slot'):
            self.build()

    def test_rejects_partial_export_and_retained_compute_on_draw(self):
        self.pages[0]['errors'] = [{'error':'read failed'}]
        with self.assertRaisesRegex(ValueError, 'incomplete'):
            self.build()
        self.pages[0]['errors'] = []
        self.pages[0]['pipelines'][0]['stages']['CS'] = self.pages[1]['pipelines'][0]['stages']['CS']
        with self.assertRaisesRegex(ValueError, 'stage'):
            self.build()


if __name__ == '__main__':
    unittest.main()
