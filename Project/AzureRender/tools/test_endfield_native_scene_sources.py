import unittest
import json
import hashlib
import subprocess
import sys
import tempfile
import types
import struct
from pathlib import Path
from unittest.mock import patch

from endfield_native.scene_sources import select_chunk_paths, select_renderer_sources
from endfield_native.scene_sources import apply_level_source_selection


class SceneSourceSelectionTests(unittest.TestCase):
    def setUp(self):
        self.paths = ['Data/Streaming/PC/a/Streaming/InitChunkData_Global_0_0.bytes',
                      'Data/Streaming/PC/a/Streaming/InitChunkData_Global_1_0.bytes',
                      'Data/Streaming/PC/a/Streaming/InitChunkData_Global_2_0.bytes']
        self.inventory = dict(chunks=[dict(path=p) for p in self.paths],
            renderers=[dict(instance=dict(entity_id=i, source=p), pairs=[dict(mesh=str(i))])
                       for i, p in enumerate(self.paths)])

    def test_default_preserves_all_sources_and_order(self):
        self.assertEqual(select_chunk_paths(self.paths), self.paths)
        rows, receipt = select_renderer_sources(self.inventory)
        self.assertEqual(rows, self.inventory['renderers'])
        self.assertEqual(receipt['excludedRenderers'], 0)

    def test_explicit_sources_preserve_inventory_order_and_draws(self):
        rows, receipt = select_renderer_sources(self.inventory, self.paths[1::-1])
        self.assertEqual([r['instance']['entity_id'] for r in rows], [0, 1])
        self.assertIs(rows[0], self.inventory['renderers'][0])
        self.assertEqual(receipt['selectedChunks'], self.paths[:2])
        self.assertEqual(receipt['excludedRenderers'], 1)
        self.assertEqual(len(self.inventory['renderers']), 3)

    def test_second_scene_uses_exact_paths(self):
        paths = ['Data/Streaming/PC/b/Streaming/InitChunkData_-2_3_0_0.bytes',
                 'Data/Streaming/PC/b/Streaming/InitChunkData_-2_3_1_0.bytes']
        self.assertEqual(select_chunk_paths(paths, paths[1:]), paths[1:])

    def test_unknown_empty_duplicate_and_noncanonical_requests_fail(self):
        for requested in ([], [self.paths[0]] * 2, ['missing.bytes'],
                          ['../bad.bytes'], ['C:/bad.bytes'], ['a\\b.bytes'],
                          ['a//b.bytes'], ['a/./b.bytes'], [False], 'text'):
            with self.subTest(requested=requested), self.assertRaises(ValueError):
                select_chunk_paths(self.paths, requested)

    def test_invalid_inventory_sources_fail_before_selection(self):
        for paths in ([self.paths[0]] * 2, [], ['../bad.bytes']):
            with self.subTest(paths=paths), self.assertRaises(ValueError):
                select_chunk_paths(paths)
        self.inventory['renderers'][0]['instance']['source'] = 'unknown.bytes'
        with self.assertRaisesRegex(ValueError, 'source'):
            select_renderer_sources(self.inventory, self.paths[1:])

    def test_selected_chunks_without_renderers_fail(self):
        self.inventory['renderers'] = self.inventory['renderers'][:1]
        with self.assertRaisesRegex(ValueError, 'renderer'):
            select_renderer_sources(self.inventory, self.paths[1:])

    def level(self):
        nodes = []
        for i in range(3):
            nodes.append(dict(id=str(i), name=f'native-{i}-draw-0', resourceId='mesh',
                components={'azure.renderable': dict(data=dict(visible=i != 1))}))
        nodes.extend([dict(id='hero', name='hero', resourceId='hero', components={}),
                      dict(id='unknown', name='native-99-draw-0', resourceId='mesh', components={})])
        return dict(schemaVersion=1, nodes=nodes, resources=[dict(id='mesh', asset='mesh.glb')])

    def test_level_filters_draws_without_activating_hidden_or_unknown_content(self):
        level = self.level()
        result, receipt = apply_level_source_selection(level, self.inventory, self.paths[:2])
        visibility = lambda x: x['components']['azure.renderable']['data']['visible']
        self.assertEqual([visibility(n) for n in result['nodes'][:3]], [True, False, False])
        self.assertTrue(visibility(level['nodes'][2]))
        self.assertEqual(result['nodes'][3:], level['nodes'][3:])
        self.assertEqual(result['resources'], level['resources'])
        self.assertEqual(receipt['changedNodes'], ['2'])
        self.assertEqual(receipt['unmappedEntities'], [99])
        repeated, again = apply_level_source_selection(result, self.inventory, self.paths[:2])
        self.assertEqual(repeated, result)
        self.assertEqual(again['changedNodes'], [])

    def test_level_rejects_conflicting_entity_sources(self):
        self.inventory['renderers'].append(dict(instance=dict(entity_id=0, source=self.paths[1])))
        with self.assertRaisesRegex(ValueError, 'conflicting'):
            apply_level_source_selection(self.level(), self.inventory, self.paths[:2])

    def test_level_requires_matching_draws(self):
        with self.assertRaisesRegex(ValueError, 'matching'):
            apply_level_source_selection(dict(schemaVersion=1, nodes=[]), self.inventory, self.paths[:2])

    def test_cli_records_hashes_and_protects_existing_outputs(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            self.inventory['format'] = 'AzureEndfieldRawSceneInventory/1'
            inventory, level = root/'inventory.json', root/'level.json'
            inventory.write_text(json.dumps(self.inventory), encoding='utf-8')
            level.write_text(json.dumps(self.level()), encoding='utf-8')
            output, receipt = root/'selected.azurelevel', root/'receipt.json'
            command = [sys.executable, str(Path(__file__).with_name('select_native_scene_sources.py')),
                       '--inventory', str(inventory), '--level', str(level), '--output', str(output),
                       '--receipt', str(receipt), '--source-chunk', self.paths[0]]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            encoded = output.read_bytes()
            audit = json.loads(receipt.read_text(encoding='utf-8'))
            self.assertEqual(audit['outputSha256'], hashlib.sha256(encoded).hexdigest())
            self.assertEqual(audit['inputs'][1]['sha256'], hashlib.sha256(level.read_bytes()).hexdigest())
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(output.read_bytes(), encoded)

    def test_cli_invalid_request_writes_no_output(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            self.inventory['format'] = 'AzureEndfieldRawSceneInventory/1'
            inventory, level = root/'inventory.json', root/'level.json'
            inventory.write_text(json.dumps(self.inventory), encoding='utf-8')
            level.write_text(json.dumps(self.level()), encoding='utf-8')
            output, receipt = root/'selected.azurelevel', root/'receipt.json'
            result = subprocess.run([sys.executable,
                str(Path(__file__).with_name('select_native_scene_sources.py')),
                '--inventory', str(inventory), '--level', str(level), '--output', str(output),
                '--receipt', str(receipt), '--source-chunk', 'unknown.bytes'], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())
            self.assertFalse(receipt.exists())

    def test_inventory_entry_filters_before_virtual_file_read(self):
        from endfield_native import scene
        from test_endfield_native_scene import empty_chunk
        from io import StringIO
        entries = [types.SimpleNamespace(file_name=p) for p in self.paths]
        reads = []
        def read_file(root, group, path):
            reads.append(path)
            return empty_chunk()
        def entities(data):
            yield dict(entity_id=1, name='source', matrix_blob=struct.pack('<16f',
                1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1), x=0,y=0,z=0)
        package = types.ModuleType('extractor_core')
        package.__path__ = []
        resources = types.ModuleType('extractor_core.resources')
        resources.load_group = lambda *a: types.SimpleNamespace(iter_files=lambda: iter(entries))
        resources.read_virtual_file = read_file
        package.resources = resources
        modules = {'extractor_core': package, 'extractor_core.resources': resources}
        for name, values in {
            'extractor_core.instances': dict(iter_entities=entities),
            'extractor_core.scene_identity': dict(validate_scene_id=lambda x: None),
            'extractor_core.projected_decals': dict(build_path_id_index=lambda x: {}),
            'extract_scene_renderer_materials': dict(component_columns=lambda x: {},
                decode_pairs=lambda *a: [], decode_draws=lambda *a: []),
        }.items():
            module = types.ModuleType(name)
            module.__dict__.update(values)
            modules[name] = module
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root/'vfs').mkdir()
            path_hash = root/'paths.json'
            path_hash.write_text('{}')
            output = root/'inventory.json'
            argv = ['scene', '--vfs-root', str(root/'vfs'), '--scene', 'a',
                '--path-hash', str(path_hash), '--bounds', '-1','1','-1','1','-1','1',
                '--output', str(output), '--source-chunk', self.paths[0]]
            with patch.dict(sys.modules, modules), patch.object(sys, 'argv', argv), patch('sys.stdout', new_callable=StringIO):
                scene.main()
            self.assertEqual(reads, self.paths[:1])
            result = json.loads(output.read_text(encoding='utf-8'))
            self.assertEqual(len(result['chunks']), 1)
            self.assertEqual(result['selection']['source_chunks']['excludedChunks'], self.paths[1:])


if __name__ == '__main__':
    unittest.main()
