import unittest
import zlib
from endfield_native.binding_projection import project_transform_bindings


def fixture():
    path='Root/Hand';crc=zlib.crc32(path.encode());absent=zlib.crc32(b'Root/Hand/Helper')
    source={'transforms':[{'path':'Actor/Root'},{'path':'Actor/'+path}]}
    avatar={'m_TOS':{str(crc):path},'m_Avatar':{'m_SkeletonNameIDArray':[0,crc]}}
    bindings=[{'type':'Transform','attribute':a,'path':p} for a in (1,2) for p in (crc,absent)]
    return {'output_track_count':2,'bindings':bindings},source,avatar,crc,absent


class BindingProjectionTests(unittest.TestCase):
    def test_default_requires_all_target_paths(self):
        audit,source,avatar,_,_=fixture()
        with self.assertRaisesRegex(ValueError,'absent'):project_transform_bindings(audit,source,avatar)

    def test_explicit_target_projection_keeps_original_track_indices(self):
        audit,source,avatar,crc,absent=fixture()
        result=project_transform_bindings(audit,source,avatar,allow_target_absent=True)
        self.assertEqual(result['boundTracks'],[{'track':0,'pathHash':crc,'sourceTransformPath':'Actor/Root/Hand'}])
        self.assertEqual(result['unboundTracks'],[{'track':1,'pathHash':absent,'positionBinding':1,'rotationBinding':3,'reason':'absent_from_target_hierarchy_and_avatar'}])

    def test_avatar_path_missing_from_hierarchy_is_rejected(self):
        audit,source,avatar,_,absent=fixture();avatar['m_TOS'][str(absent)]='Root/Hand/Helper'
        with self.assertRaisesRegex(ValueError,'Avatar'):project_transform_bindings(audit,source,avatar,allow_target_absent=True)

    def test_target_skeleton_id_is_rejected_even_without_path_string(self):
        audit,source,avatar,_,absent=fixture();avatar['m_Avatar']['m_SkeletonNameIDArray'].append(absent)
        with self.assertRaisesRegex(ValueError,'Avatar'):project_transform_bindings(audit,source,avatar,allow_target_absent=True)

    def test_ambiguous_suffix_is_rejected(self):
        audit,source,avatar,_,_=fixture();source['transforms'].append({'path':'Other/Root/Hand'})
        with self.assertRaisesRegex(ValueError,'ambiguous'):project_transform_bindings(audit,source,avatar,allow_target_absent=True)

    def test_misaligned_rotation_block_is_rejected(self):
        audit,source,avatar,_,_=fixture();audit['bindings'][2]['path']=99
        with self.assertRaisesRegex(ValueError,'align'):project_transform_bindings(audit,source,avatar,allow_target_absent=True)

    def test_projection_cannot_leave_every_track_unbound(self):
        audit,source,avatar,_,_=fixture();source['transforms']=[];avatar['m_TOS']={};avatar['m_Avatar']['m_SkeletonNameIDArray']=[]
        with self.assertRaisesRegex(ValueError,'bound'):project_transform_bindings(audit,source,avatar,allow_target_absent=True)


if __name__=='__main__':unittest.main()
