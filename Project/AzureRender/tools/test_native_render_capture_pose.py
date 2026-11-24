"""Freeze named captured bone poses through ordinary glTF node transforms."""
import copy
import unittest
import numpy as np
from endfield_native import capture_skin_frame as subject


class CapturePoseTests(unittest.TestCase):
    def test_bakes_actual_palette_with_weighted_positions_and_directions(self):
        palette=np.repeat(np.eye(4)[None,:,:],2,axis=0)
        palette[0,:3,3]=[1,0,0];palette[1,:3,3]=[0,2,0]
        result=subject.bake_skin_streams(positions=[[0,0,0]],normals=[[0,0,1]],
            tangents=[[1,0,0,-1]],joints=[[0,1,0,0]],weights=[[.25,.75,0,0]],matrices=palette)
        np.testing.assert_allclose(result['positions'],[[.25,1.5,0]])
        np.testing.assert_allclose(result['normals'],[[0,0,1]])
        np.testing.assert_allclose(result['tangents'],[[1,0,0,-1]])
        for weights in ([[0,0,0,0]],[[2,0,0,0]],[[float('nan'),0,0,0]]):
            with self.assertRaises(ValueError):subject.bake_skin_streams(positions=[[0,0,0]],normals=[[0,0,1]],
                tangents=[[1,0,0,1]],joints=[[0,0,0,0]],weights=weights,matrices=palette)

    def fixture(self):
        return {'nodes':[{'name':'root','translation':[0,2,0],'children':[1]},
            {'name':'child','translation':[1,0,0]}], 'animations':[{'name':'idle'}],
            'meshes':[{'primitives':[]}],'skins':[{'joints':[0,1]}]}

    def test_resolves_child_against_captured_parent_and_preserves_geometry(self):
        model=self.fixture();original=copy.deepcopy(model)
        parent=np.eye(4);parent[:3,3]=[4,5,6]
        child=np.eye(4);child[:3,3]=[5,7,6]
        result,report=subject.freeze_gltf_pose(model,{'root':parent,'child':child},freeze_animations=True)
        self.assertEqual(result['nodes'][1]['matrix'][12:15],[1,2,0])
        self.assertEqual(result['nodes'][0]['matrix'][12:15],[4,5,6])
        self.assertNotIn('animations',result)
        self.assertEqual(result['meshes'],original['meshes']);self.assertEqual(model,original)
        self.assertEqual(report['matchedNodes'],2)

    def test_unmapped_children_keep_local_pose(self):
        model=self.fixture();model.pop('animations')
        result,_=subject.freeze_gltf_pose(model,{'root':np.eye(4)})
        self.assertEqual(result['nodes'][1],model['nodes'][1])

    def test_rejects_implicit_animation_removal_and_unknown_or_duplicate_nodes(self):
        with self.assertRaises(ValueError):subject.freeze_gltf_pose(self.fixture(),{'root':np.eye(4)})
        for name in ('missing','child'):
            model=self.fixture();model['nodes'].append({'name':'child'})
            with self.assertRaises(ValueError):subject.freeze_gltf_pose(model,{name:np.eye(4)},freeze_animations=True)

    def test_rejects_cycles_multiple_parents_and_invalid_matrices(self):
        for extra in ({'children':[0]},{'children':[1]}):
            model=self.fixture();model['nodes'][1].update(extra)
            with self.assertRaises(ValueError):subject.freeze_gltf_pose(model,{'root':np.eye(4)},freeze_animations=True)
        for value in (np.zeros((4,4)),np.full((4,4),np.nan),np.eye(3)):
            with self.assertRaises(ValueError):subject.freeze_gltf_pose(self.fixture(),{'root':value},freeze_animations=True)


if __name__=='__main__':unittest.main()
