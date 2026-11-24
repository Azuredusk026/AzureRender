import math
import struct
import unittest
from endfield_native.reflections import decode_probe_fields, merge_records


class ReflectionTests(unittest.TestCase):
    def fields(self):
        return {0x10000000:(9,struct.pack('<3f',8,10,12)),
                0x10010000:(9,struct.pack('<3f',0,1,2)),
                0x10040000:(17,struct.pack('<16f',*range(16))),
                0x10050000:(17,struct.pack('<16f',*range(16))),
                0x100b0000:(17,struct.pack('<27f',*range(27))),
                0x100c0000:(16,b'Assets/example.exr'),
                0x100f0000:(9,struct.pack('<3f',4,5,6)),
                0x12345678:(99,b'unknown')}

    def test_full_payload_and_unverified_semantics(self):
        r=decode_probe_fields(self.fields())
        self.assertEqual(r['coefficient_candidate'],list(range(27)))
        self.assertEqual(r['texture_path'],'Assets/example.exr')
        self.assertEqual(r['runtime_activation'],'unverified')
        self.assertEqual(r['coefficient_basis'],'unverified')
        self.assertEqual(r['raw_fields']['0x12345678']['hex'],b'unknown'.hex())

    def test_invalid_type_size_and_nonfinite(self):
        for v in ((9,bytes(108)),(17,bytes(104)),(17,struct.pack('<27f',math.nan,*range(26)))):
            f=self.fields();f[0x100b0000]=v
            with self.assertRaises(ValueError):decode_probe_fields(f)

    def test_identity_is_scene_and_entity(self):
        a={'scene':'a','entity_id':7,'position':[1,2,3],'probe':decode_probe_fields(self.fields()),'source':'a0'}
        b=dict(a,scene='b',source='b0')
        self.assertEqual(len(merge_records([a,b,a])),2)
        with self.assertRaises(ValueError):merge_records([a,dict(a,position=[0,0,0])])

    def test_missing_required_field(self):
        f=self.fields();del f[0x100c0000]
        with self.assertRaises(ValueError):decode_probe_fields(f)

    def test_absent_coefficients_remain_explicit(self):
        f=self.fields();del f[0x100b0000]
        self.assertIsNone(decode_probe_fields(f)['coefficient_candidate'])


if __name__=='__main__':unittest.main()
