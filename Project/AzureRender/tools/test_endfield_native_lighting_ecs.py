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
    struct.pack_into('<f',light,444,-1)
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
        self.assertEqual(value['falloff_exponent'],-1)

    def test_native_falloff_exponent(self):
        common,light=fixture();bad=bytearray(light)
        struct.pack_into('<f',bad,444,6.24)
        self.assertAlmostEqual(lighting_ecs.decode_buffers(common,bad)['falloff_exponent'],6.24,places=5)
        struct.pack_into('<f',bad,444,float('nan'))
        with self.assertRaises(ValueError):lighting_ecs.decode_buffers(common,bad)

    def test_native_obb_fields_preserve_three_component_orientation(self):
        common,light=fixture();light=bytearray(light)
        struct.pack_into('<10f',light,100,3,4,5,6,7,8,10,20,30,.75)
        value=lighting_ecs.decode_buffers(common,light)
        self.assertEqual(value['obb_relative_position'],[3,4,5])
        self.assertEqual(value['obb_half_extents'],[6,7,8])
        self.assertEqual(value['obb_orientation'],[10,20,30])
        self.assertEqual(value['obb_falloff_threshold'],.75)

    def test_native_obb_nonfinite_fields_are_rejected(self):
        common,light=fixture()
        for offset in (100,112,124,136):
            invalid=bytearray(light);struct.pack_into('<f',invalid,offset,float('nan'))
            with self.assertRaises(ValueError):lighting_ecs.decode_buffers(common,invalid)

    def test_render_box_preserves_native_relative_world_space(self):
        common,light=fixture();fields=lighting_ecs.decode_buffers(common,light)
        fields.update(obb_relative_position=[1,2,3],obb_half_extents=[2,4,8],
                      obb_orientation=[0,0,0],obb_falloff_threshold=.5)
        box=lighting_ecs.render_influence_box(fields)
        self.assertEqual(box['worldToBox'],[[.5,0,0,-2],[0,.25,0,-1.5],[0,0,.125,-1]])
        self.assertEqual(box['falloffThreshold'],.5)
        fields['obb_orientation']=[0,0,90]
        box=lighting_ecs.render_influence_box(fields)
        self.assertEqual(box['worldToBox'],[[0,.5,0,-3],[-.25,0,0,1],[0,0,.125,-1]])
        fields.update(position=[0,0,0],obb_relative_position=[0,0,0],
                      obb_half_extents=[1,1,1],obb_orientation=[90,90,0])
        self.assertEqual(lighting_ecs.render_influence_box(fields)['worldToBox'],
                         [[0,0,-1,0],[1,0,0,0],[0,-1,0,0]])
        fields['enable_obb']=False
        self.assertEqual(lighting_ecs.render_influence_box(fields),{'enabled':False})

    def test_render_box_rejects_unrepresentable_source(self):
        common,light=fixture();fields=lighting_ecs.decode_buffers(common,light)
        fields.update(obb_half_extents=[1,1,0],obb_falloff_threshold=.5)
        with self.assertRaises(ValueError):lighting_ecs.render_influence_box(fields)

    def test_render_falloff_mapping(self):
        self.assertEqual(lighting_ecs.render_falloff(-1),dict(falloff='smooth-inverse-square',falloffExponent=2))
        self.assertEqual(lighting_ecs.render_falloff(6.24),dict(falloff='range-power',falloffExponent=6.24))
        for invalid in (0,65,float('inf'),float('nan')):
            with self.assertRaises(ValueError):lighting_ecs.render_falloff(invalid)

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
