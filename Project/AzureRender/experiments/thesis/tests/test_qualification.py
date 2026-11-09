import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qualification', ROOT / 'tools/qualification.py')


class QualificationTests(unittest.TestCase):
    def evaluate(self, device):
        self.assertTrue(Path(spec.origin).is_file(), 'qualification evaluator must exist')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module.evaluate(device)

    def device(self):
        return dict(apiVersion=4206592, extensions=[], dynamicRendering=True,
                    dynamicRenderingLocalRead=True, maxColorAttachments=8,
                    maxPerStageDescriptorInputAttachments=8,
                    queues=[dict(index=0, graphics=True, timestampValidBits=64)],
                    formats=[dict(name=n, supported=True, maxWidth=4096,
                                  maxHeight=4096, sampleCounts=1)
                             for n in ['albedo', 'normalDepth', 'depth', 'toon', 'output']])

    def test_feature_bit_does_not_replace_core_or_extension_support(self):
        result = self.evaluate(self.device())
        self.assertTrue(result['traditional'])
        self.assertTrue(result['subpasses'])
        self.assertFalse(result['drlr'])

    def test_extension_and_feature_allow_color_only_single_sample_workload(self):
        device = self.device()
        device['extensions'] = ['VK_KHR_dynamic_rendering_local_read']
        result = self.evaluate(device)
        self.assertTrue(result['drlr'])
        self.assertEqual(result['queueFamily'], 0)

    def test_missing_feature_is_rejected_even_with_extension(self):
        device = self.device()
        device.update(extensions=['VK_KHR_dynamic_rendering_local_read'],
                      dynamicRenderingLocalRead=False)
        self.assertFalse(self.evaluate(device)['drlr'])

    def test_missing_format_or_timestamp_prevents_timing_qualification(self):
        for change in ['format', 'timestamp', 'extent', 'input-limit']:
            with self.subTest(change=change):
                device = self.device()
                if change == 'format': device['formats'].pop()
                if change == 'timestamp': device['queues'][0]['timestampValidBits'] = 0
                if change == 'extent': device['formats'][0]['maxWidth'] = 1024
                if change == 'input-limit': device['maxPerStageDescriptorInputAttachments'] = 1
                self.assertFalse(self.evaluate(device)['subpasses'])

    def test_core_14_can_support_drlr_without_extension(self):
        device = self.device()
        device['apiVersion'] = 4210688
        self.assertTrue(self.evaluate(device)['drlr'])


if __name__ == '__main__':
    unittest.main()
