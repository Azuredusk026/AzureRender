"""Check authored material colors against captured GPU parameter uploads."""
from pathlib import Path
import copy
import json
import math
import unittest
from blender.native_surface_colors import surface_color_record


class SurfaceColorTests(unittest.TestCase):
    def test_captured_uploads(self):
        fixture=Path(__file__).resolve().parents[1]/'tests/fixtures/native_surface_color_upload.json'
        cases=json.loads(fixture.read_text(encoding='utf-8'))['cases']
        for case in cases:
            with self.subTest(event=case['event']):
                record=dict(Name=case['name'],Colors=case['authoredColors'],Floats={'_Metallic':.7})
                before=copy.deepcopy(record);result=surface_color_record(record,'authored-srgb')
                self.assertEqual(record,before)
                self.assertEqual(result['SurfaceColorEncoding'],'gpu-linear')
                for name,expected in case['gpuColors'].items():
                    self.assertLess(max(abs(a-b) for a,b in zip(result['Colors'][name],expected)),1e-6)
                    self.assertEqual(result['Colors'][name][3],record['Colors'][name][3])
                self.assertEqual(result['Floats'],record['Floats'])

    def test_explicit_linear_values_and_ownership(self):
        record={'Colors':{'_BaseColor':[.2,.4,2,.37],'_EmissiveColor':[4,5,6,1]}}
        result=surface_color_record(record,'gpu-linear')
        self.assertEqual(result['Colors'],record['Colors'])
        result['Colors']['_BaseColor'][0]=.9
        self.assertEqual(record['Colors']['_BaseColor'][0],.2)
        self.assertEqual(surface_color_record(record,'authored-srgb')['Colors']['_EmissiveColor'],[4,5,6,1])

    def test_invalid_values(self):
        for value in [float('nan'),float('inf'),-float('inf')]:
            with self.assertRaises(ValueError):surface_color_record({'Colors':{'_BaseColor':[value,0,0,1]}},'authored-srgb')
        with self.assertRaises(ValueError):surface_color_record({'Colors':{'_BaseColor':[1,1,1]}},'authored-srgb')
        with self.assertRaises(ValueError):surface_color_record({'Colors':{}},'implicit')
        with self.assertRaises(ValueError):surface_color_record({'Colors':{},'SurfaceColorEncoding':'gpu-linear'},'authored-srgb')


if __name__=='__main__':unittest.main()
