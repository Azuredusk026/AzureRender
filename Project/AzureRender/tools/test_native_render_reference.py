import copy
import hashlib
import tempfile
import unittest
from pathlib import Path
from PIL import Image

try:
    from native_render.reference_manifest import validate_manifest
except ImportError:
    validate_manifest = None


class ReferenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        Image.new('RGB', (64, 32), (24, 80, 120)).save(self.root / 'frame.png')
        self.document = {
            'format': 'NativeRenderReference/1', 'game_version': '1.0.14',
            'entries': [{'id': 'front', 'role': 'game_reference', 'status': 'captured',
                'game_version': '1.0.14', 'path': 'frame.png',
                'sha256': hashlib.sha256((self.root / 'frame.png').read_bytes()).hexdigest(),
                'state': {'scene': 'synthetic-room', 'quest': 'fixture',
                    'lighting': 'day', 'quality': 'high', 'resolution': [64, 32],
                    'camera': {'position': [0, 1, 2], 'rotation': [0, 0, 0, 1], 'fov_y': 60},
                    'exposure': 0, 'antialiasing': 'none', 'time': 0},
                'source': 'local-game'}]}

    def check(self, doc):
        self.assertIsNotNone(validate_manifest, 'reference validator is not implemented')
        return validate_manifest(doc, self.root)

    def test_captured_reference_is_eligible(self):
        self.assertEqual(self.check(self.document)['eligible'], ['front'])

    def test_observed_frame_is_hashed_but_pending_calibration(self):
        row = self.document['entries'][0]
        row.update(status='observed', reason='camera and quality calibration pending')
        del row['state']['camera']
        self.assertEqual(self.check(self.document)['pending'], ['front'])
        self.assertEqual(self.check(self.document)['eligible'], [])
        (self.root / 'frame.png').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'sha256'):
            self.check(self.document)

    def test_missing_reference_is_pending_not_eligible(self):
        self.document['entries'] = [{'id': 'front', 'role': 'game_reference',
            'status': 'missing', 'reason': 'game capture pending'}]
        self.assertEqual(self.check(self.document), {'eligible': [], 'pending': ['front'], 'auxiliary': []})

    def test_missing_state_is_rejected(self):
        for key in self.document['entries'][0]['state']:
            with self.subTest(key=key):
                doc = copy.deepcopy(self.document)
                del doc['entries'][0]['state'][key]
                with self.assertRaisesRegex(ValueError, 'state'):
                    self.check(doc)

    def test_digest_mismatch_is_rejected(self):
        (self.root / 'frame.png').write_bytes(b'changed frame')
        with self.assertRaisesRegex(ValueError, 'sha256'):
            self.check(self.document)

    def test_mixed_version_is_rejected(self):
        self.document['entries'][0]['game_version'] = '1.0.15'
        with self.assertRaisesRegex(ValueError, 'version'):
            self.check(self.document)

    def test_engine_and_network_images_are_auxiliary(self):
        for role in ('engine_baseline', 'network_reference'):
            self.document['entries'][0]['role'] = role
            self.assertEqual(self.check(self.document)['auxiliary'], ['front'])
            self.assertEqual(self.check(self.document)['eligible'], [])

    def test_duplicate_identity_and_unknown_format_rejected(self):
        self.document['entries'].append(copy.deepcopy(self.document['entries'][0]))
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            self.check(self.document)
        self.document['format'] = 'NativeRenderReference/2'
        with self.assertRaisesRegex(ValueError, 'format'):
            self.check(self.document)

    def test_invalid_camera_rejected(self):
        self.document['entries'][0]['state']['camera']['fov_y'] = float('nan')
        with self.assertRaisesRegex(ValueError, 'camera'):
            self.check(self.document)

    def test_declared_resolution_must_match_image_pixels(self):
        self.document['entries'][0]['state']['resolution'] = [1920, 1080]
        with self.assertRaisesRegex(ValueError, 'resolution'):
            self.check(self.document)

    def test_digest_does_not_qualify_non_image_or_truncated_image(self):
        valid = (self.root / 'frame.png').read_bytes()
        for payload in (b'not an image', valid[:48]):
            with self.subTest(payload=payload[:12]):
                (self.root / 'frame.png').write_bytes(payload)
                self.document['entries'][0]['sha256'] = hashlib.sha256(payload).hexdigest()
                with self.assertRaisesRegex(ValueError, 'image'):
                    self.check(self.document)

    def test_observed_image_reports_actual_encoding_and_dimensions(self):
        from native_render.reference_manifest import inspect_reference_image
        Image.new('RGB', (91, 47)).save(self.root / 'frame.png', format='JPEG')
        result = inspect_reference_image(self.root / 'frame.png')
        self.assertEqual(result, {'format': 'JPEG', 'resolution': [91, 47]})

    def engine_recipe(self):
        self.document['entries'][0]['role'] = 'engine_baseline'
        def record(name, contents):
            path = self.root / name
            path.write_bytes(contents)
            return dict(path=name, bytes=len(contents),
                        sha256=hashlib.sha256(contents).hexdigest())
        recipe = dict(schemaVersion=1, workingDirectory='.',
                      program=record('player.exe', b'fixed player version'),
                      project=record('project.azureproject', b'{"startupScene":"assets:/scene.azurelevel"}'),
                      inputs=[record('scene.azurelevel', b'fixed camera and lighting'),
                              record('material.azurematerial', b'dedicated eye surface')],
                      command=['player.exe', '--project', 'project.azureproject',
                               '--width', '64', '--height', '32', '--capture-frames', '32',
                               '--set', 'render.antiAliasing=3'])
        self.document['entries'][0]['reproduction'] = recipe
        return recipe

    def test_engine_input_lock_accepts_intact_inputs_and_rejects_material_drift(self):
        self.engine_recipe()
        self.assertEqual(self.check(self.document)['auxiliary'], ['front'])
        path = self.root / 'material.azurematerial'
        path.write_bytes(b'X' * path.stat().st_size)
        with self.assertRaisesRegex(ValueError, 'reproduction.*material'):
            self.check(self.document)

    def test_engine_input_lock_rejects_program_and_project_drift(self):
        recipe = self.engine_recipe()
        for role in ('program', 'project'):
            with self.subTest(role=role):
                path = self.root / recipe[role]['path']
                original = path.read_bytes()
                path.write_bytes(original + b'changed')
                with self.assertRaisesRegex(ValueError, 'reproduction.*' + role):
                    self.check(self.document)
                path.write_bytes(original)

    def test_engine_input_lock_rejects_command_using_another_project(self):
        recipe = self.engine_recipe()
        (self.root / 'other.azureproject').write_text('{}')
        recipe['command'][2] = 'other.azureproject'
        with self.assertRaisesRegex(ValueError, 'reproduction.*command.*project'):
            self.check(self.document)

    def test_engine_input_lock_rejects_command_using_another_program(self):
        recipe = self.engine_recipe()
        (self.root / 'other.exe').write_bytes(b'another program')
        recipe['command'][0] = 'other.exe'
        with self.assertRaisesRegex(ValueError, 'reproduction.*command.*program'):
            self.check(self.document)

    def test_engine_input_lock_resolves_command_against_working_directory(self):
        recipe = self.engine_recipe()
        (self.root / 'work').mkdir()
        recipe['workingDirectory'] = 'work'
        recipe['command'][0] = '../player.exe'
        recipe['command'][2] = '../project.azureproject'
        self.assertEqual(self.check(self.document)['auxiliary'], ['front'])

    def test_engine_input_lock_rejects_missing_input_and_duplicate_alias(self):
        recipe = self.engine_recipe()
        (self.root / 'material.azurematerial').unlink()
        with self.assertRaisesRegex(ValueError, 'reproduction.*material'):
            self.check(self.document)
        recipe['inputs'].pop()
        duplicate = copy.deepcopy(recipe['inputs'][0])
        duplicate['path'] = './scene.azurelevel'
        recipe['inputs'].append(duplicate)
        with self.assertRaisesRegex(ValueError, 'reproduction.*duplicate'):
            self.check(self.document)

    def test_engine_input_lock_rejects_incomplete_or_invalid_recipe(self):
        recipe = self.engine_recipe()
        for field in tuple(recipe):
            with self.subTest(missing=field):
                doc = copy.deepcopy(self.document)
                del doc['entries'][0]['reproduction'][field]
                with self.assertRaisesRegex(ValueError, 'reproduction'):
                    self.check(doc)
        for field, value in [('schemaVersion', 2), ('schemaVersion', True),
                             ('workingDirectory', 'missing'), ('inputs', []),
                             ('command', 'player.exe --project project.azureproject'),
                             ('command', ['player.exe', '--project']),
                             ('command', ['player.exe', '--project', 'project.azureproject',
                                          '--project', 'project.azureproject'])]:
            with self.subTest(field=field, value=value):
                doc = copy.deepcopy(self.document)
                doc['entries'][0]['reproduction'][field] = value
                with self.assertRaisesRegex(ValueError, 'reproduction'):
                    self.check(doc)

    def test_engine_input_lock_rejects_wrong_declared_size_and_non_engine_role(self):
        recipe = self.engine_recipe()
        recipe['program']['bytes'] += 1
        with self.assertRaisesRegex(ValueError, 'reproduction.*program'):
            self.check(self.document)
        recipe['program']['bytes'] -= 1
        self.document['entries'][0]['role'] = 'game_reference'
        with self.assertRaisesRegex(ValueError, 'reproduction.*engine_baseline'):
            self.check(self.document)


if __name__ == '__main__':
    unittest.main()
