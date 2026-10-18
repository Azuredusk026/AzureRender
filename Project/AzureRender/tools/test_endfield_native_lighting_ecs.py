import math
import struct
import unittest
from endfield_native import lighting_ecs


def fixture():
    common=bytearray(236);light=bytearray(448)
    struct.pack_into('<4f',common,0,.8,.5,.25,1)
    struct.pack_into('<ffB',common,16,40,6500,1)
    matrix=[1,0,0,0,0,1,0,0,0,0,1,0,3,4,5,1]
    struct.pack_into('<16f',common,108,*matrix)
    struct.pack_into('<ii',light,12,0,0)
    struct.pack_into('<f',light,24,15)
    struct.pack_into('<ff',light,36,100,50)
    struct.pack_into('<f',light,144,-1)
    light[92]=1
    # Native records contain legitimate packed sentinels outside decoded fields.
    struct.pack_into('<I',light,324,0x7fc00000)
    return bytes(common),bytes(light)


class EcsLightTests(unittest.TestCase):
    def test_native_getter_offsets_and_matrix(self):
        common,light=fixture();value=lighting_ecs.decode_buffers(common,light)
        self.assertEqual((value['light_type'],value['shape']),(0,0))
        self.assertEqual((value['intensity'],value['range']),(40,15))
        self.assertEqual((value['spot_angle'],value['inner_spot_angle']),(100,50))
        self.assertEqual(value['position'],[3,4,5]);self.assertEqual(value['direction'],[0,0,1])
        self.assertTrue(value['enable_obb']);self.assertTrue(value['use_color_temperature'])
        self.assertAlmostEqual(value['color'][0],.8)

    def test_sizes_and_semantic_nonfinite_rejected(self):
        common,light=fixture()
        for c,l in ((common[:-1],light),(common,light+b'\0')):
            with self.assertRaises(ValueError):lighting_ecs.decode_buffers(c,l)
        bad=bytearray(common);struct.pack_into('<f',bad,16,float('nan'))
        with self.assertRaises(ValueError):lighting_ecs.decode_buffers(bad,light)
        bad=bytearray(light);struct.pack_into('<f',bad,24,-1)
        with self.assertRaises(ValueError):lighting_ecs.decode_buffers(common,bad)

    def test_duplicate_requires_equal_serialized_properties(self):
        rows=[dict(entity_id=4,identity='a',sources=[{'path':'x'}]),dict(entity_id=4,identity='a',sources=[{'path':'y'}])]
        merged=lighting_ecs.merge_rows(rows)
        self.assertEqual(len(merged),1);self.assertEqual(merged[0]['sources'],[{'path':'x'},{'path':'y'}])
        rows[1]['identity']='b'
        with self.assertRaisesRegex(ValueError,'conflicting'):lighting_ecs.merge_rows(rows)


if __name__=='__main__':unittest.main()
