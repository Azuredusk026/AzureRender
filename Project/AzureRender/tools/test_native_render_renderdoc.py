"""Capture receipts must prove their outputs and preserve failure evidence."""
import copy
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from types import SimpleNamespace

try:
    from native_render.renderdoc_capture import verify_receipt, run_evidence
except ImportError:
    verify_receipt = run_evidence = None

try:
    from native_render.renderdoc_replay import action_stage_scope, active_stage_reflections
except ImportError:
    action_stage_scope = active_stage_reflections = None

try:
    from native_render.renderdoc_replay import sampler_binding
except ImportError:
    sampler_binding = None


class RenderDocSamplerTests(unittest.TestCase):
    def test_sampler_uses_sampler_descriptor_and_retains_sampling_boundary(self):
        self.assertTrue(callable(sampler_binding), 'Sampler evidence exporter is required')
        sampler = SimpleNamespace(object='sampler-338', addressU='Wrap', addressV='Wrap',
            addressW='Wrap', filter=SimpleNamespace(minify='Linear', magnify='Linear',
                mip='Point', filter='Normal'), mipBias=0., minLOD=0., maxLOD=1000.,
            maxAnisotropy=0., compareFunction='AlwaysTrue', unnormalized=False)
        used = SimpleNamespace(sampler=sampler, descriptor=None,
            access=SimpleNamespace(index=1, arrayElement=0))
        result = sampler_binding(used)
        self.assertEqual(result['object'], 'sampler-338')
        self.assertEqual(result['filter'], dict(minify='Linear', magnify='Linear',
            mip='Point', function='Normal'))
        self.assertEqual(result['address'], ['Wrap', 'Wrap', 'Wrap'])
        self.assertEqual(result['lod'], dict(bias=0., minimum=0., maximum=1000.))
        self.assertEqual(result['maximum_anisotropy'], 0.)
        self.assertEqual(result['shader_binding_index'], 1)
        self.assertFalse(result['unnormalized'])
        sampler.addressU = 'ClampEdge'
        sampler.filter.mip = 'Linear'
        sampler.mipBias = -1.
        sampler.minLOD = 2.
        sampler.maxLOD = 7.
        sampler.maxAnisotropy = 8.
        result = sampler_binding(used)
        self.assertEqual(result['address'][0], 'ClampEdge')
        self.assertEqual(result['filter']['mip'], 'Linear')
        self.assertEqual(result['lod'], dict(bias=-1., minimum=2., maximum=7.))
        self.assertEqual(result['maximum_anisotropy'], 8.)


class RenderDocStageActivityTests(unittest.TestCase):
    def test_draw_and_dispatch_do_not_export_retained_inactive_bindings(self):
        self.assertTrue(callable(active_stage_reflections), 'Active shader filtering is required')
        rd = SimpleNamespace(ShaderStage=SimpleNamespace(Vertex='vertex', Pixel='pixel', Compute='compute'))
        for action, expected, forbidden in (
                ({'draw': True}, ['vertex', 'pixel'], 'compute'),
                ({'dispatch': True}, ['compute'], 'vertex'),
                ({'mesh_draw': True, 'draw': True}, ['pixel'], 'vertex'),
                ({'ray_dispatch': True}, [], 'compute'),
                ({}, [], 'vertex')):
            with self.subTest(action=action):
                queries = []
                def reflection(stage):
                    queries.append(stage)
                    return 'retained-' + stage
                pipeline = SimpleNamespace(GetShaderReflection=reflection)
                exported = list(active_stage_reflections(rd, pipeline, action))
                self.assertEqual([stage for stage, value in exported], expected)
                self.assertEqual(queries, expected)
                self.assertNotIn(forbidden, queries)

    def test_stage_coverage_does_not_claim_uninspected_graphics_or_ray_stages(self):
        self.assertTrue(callable(action_stage_scope), 'Event semantics are required')
        scope = action_stage_scope({'draw': True})
        self.assertEqual(scope['kind'], 'draw')
        self.assertEqual(scope['export_candidates'], ['Vertex', 'Pixel'])
        self.assertEqual(scope['uninspected_stages'], ['Hull', 'Domain', 'Geometry'])
        mesh = action_stage_scope({'mesh_draw': True})
        self.assertEqual(mesh['kind'], 'mesh_draw')
        self.assertEqual(mesh['uninspected_stages'], ['Task', 'Mesh'])
        ray = action_stage_scope({'ray_dispatch': True})
        self.assertEqual(ray['kind'], 'ray_dispatch')
        self.assertEqual(ray['export_candidates'], [])
        self.assertIn('RayGen', ray['uninspected_stages'])


class RenderDocEvidenceTests(unittest.TestCase):
    def fixture(self, root):
        source = root / 'source.rdc'
        source.write_bytes(b'source-capture')
        output = root / 'evidence'
        output.mkdir()
        target = output / 'event-15.png'
        target.write_bytes(b'exported-image')
        source_record = {'path': str(source.resolve()), 'bytes': 14,
            'sha256': hashlib.sha256(b'source-capture').hexdigest()}
        artifact = {'path': str(target.resolve()), 'bytes': 14,
            'sha256': hashlib.sha256(b'exported-image').hexdigest()}
        receipt = {'format': 'RenderDocEvidence/1', 'operation': 'replay',
            'status': 'replayed', 'source': source_record,
            'tool_version': '1.43', 'api': 'GraphicsAPI.Vulkan',
            'action_count': 12, 'draw_count': 2, 'dispatch_count': 0,
            'artifacts': [artifact]}
        return source, output, receipt

    def test_accepts_matching_replayed_source_and_real_export(self):
        self.assertTrue(callable(verify_receipt), 'Receipt verifier is required')
        with tempfile.TemporaryDirectory() as folder:
            source, output, receipt = self.fixture(Path(folder))
            verify_receipt(receipt, 'replay', source, output)

    def test_rejects_missing_changed_and_outside_output_artifacts(self):
        self.assertTrue(callable(verify_receipt), 'Receipt verifier is required')
        for problem in ('missing', 'changed', 'outside', 'duplicate'):
            with self.subTest(problem=problem), tempfile.TemporaryDirectory() as folder:
                source, output, receipt = self.fixture(Path(folder))
                path = Path(receipt['artifacts'][0]['path'])
                if problem == 'missing':
                    path.unlink()
                elif problem == 'changed':
                    path.write_bytes(b'different-data')
                elif problem == 'outside':
                    moved = output.parent / path.name
                    path.rename(moved)
                    receipt['artifacts'][0]['path'] = str(moved)
                else:
                    receipt['artifacts'].append(copy.deepcopy(receipt['artifacts'][0]))
                with self.assertRaises(ValueError):
                    verify_receipt(receipt, 'replay', source, output)

    def test_rejects_mixed_source_empty_actions_and_failed_replay(self):
        self.assertTrue(callable(verify_receipt), 'Receipt verifier is required')
        for problem in ('source-digest', 'source-path', 'empty', 'failed', 'operation', 'artifacts'):
            with self.subTest(problem=problem), tempfile.TemporaryDirectory() as folder:
                source, output, receipt = self.fixture(Path(folder))
                if problem == 'source-digest':
                    source.write_bytes(b'updated-source')
                elif problem == 'source-path':
                    receipt['source']['path'] = str(output / 'unrelated.rdc')
                elif problem == 'empty':
                    receipt['action_count'] = 0
                elif problem == 'failed':
                    receipt['status'] = 'failed'
                elif problem == 'operation':
                    receipt['operation'] = 'capture'
                else:
                    receipt['artifacts'] = []
                with self.assertRaises(ValueError):
                    verify_receipt(receipt, 'replay', source, output)

    def test_existing_output_and_missing_inputs_never_launch_a_target(self):
        self.assertTrue(callable(run_evidence), 'External runtime driver is required')
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            output = root / 'keep'
            output.mkdir()
            preserved = output / 'user-data.txt'
            preserved.write_text('keep', encoding='utf8')
            with self.assertRaises(ValueError):
                run_evidence('capture', python=root/'missing.exe', renderdoc_dir=root,
                    source=root/'missing-target.exe', output=output)
            self.assertEqual(preserved.read_text(encoding='utf8'), 'keep')
            with self.assertRaises(ValueError):
                run_evidence('replay', python=root/'missing.exe', renderdoc_dir=root,
                    source=root/'missing.rdc', output=root/'new-output')
            self.assertFalse((root/'new-output').exists())

    def test_malformed_receipt_is_a_validation_failure(self):
        self.assertTrue(callable(verify_receipt), 'Receipt verifier is required')
        with tempfile.TemporaryDirectory() as folder:
            source, output, valid = self.fixture(Path(folder))
            for malformed in (None, [], 'success', 1):
                with self.subTest(receipt=malformed), self.assertRaises(ValueError):
                    verify_receipt(malformed, 'replay', source, output)
            valid['artifacts'][0]['bytes'] = True
            with self.assertRaises(ValueError):
                verify_receipt(valid, 'replay', source, output)

    def test_broken_external_binding_retains_failure_without_starting_target(self):
        self.assertTrue(callable(run_evidence), 'External runtime driver is required')
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            runtime = root/'broken-renderdoc'
            runtime.mkdir()
            (runtime/'renderdoc.pyd').write_bytes(b'broken-extension')
            (runtime/'renderdoc.dll').write_bytes(b'broken-library')
            target = root/'unlaunched.exe'
            target.write_bytes(b'not-executable')
            result = run_evidence('capture', python=Path(sys.executable),
                renderdoc_dir=runtime, source=target, output=root/'evidence')
            self.assertEqual(result['status'], 'failed')
            self.assertIn('error', result)
            self.assertIn('driver_error', result)
            self.assertTrue((root/'evidence/receipt.json').is_file())
            self.assertEqual(target.read_bytes(), b'not-executable')


if __name__ == '__main__':
    unittest.main()
