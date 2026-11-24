"""Source weights must match geometry without inventing influences."""
import unittest
from restore_skin_weights import SourceWeights

class SourceWeightTests(unittest.TestCase):
    def test_exact_match_and_source_bone_name_mapping(self):
        source = SourceWeights({'meshes':[{'bones':['hand','spine'], 'vertices':[
            {'position':[0,1,0], 'weights':[[0,.8],[1,.2]]}], 'corners':[[0,.2,.3]]}]})
        self.assertEqual(source.match([0,1,0],[.2,.3]), {'hand':.8,'spine':.2})
        with self.assertRaises(ValueError): source.match([0,2,0],[.2,.3])

    def test_seam_uses_uv_and_ambiguous_source_rejected(self):
        meshes=[{'bones':['left','right'],'vertices':[
            {'position':[0,1,0],'weights':[[0,1]]},
            {'position':[0,1,0],'weights':[[1,1]]}], 'corners':[[0,.2,.3],[1,.8,.3]]}]
        source=SourceWeights({'meshes':meshes})
        self.assertEqual(source.match([0,1,0],[.8,.3]),{'right':1})
        meshes[0]['corners'][1]=[1,.2,.3]
        with self.assertRaises(ValueError): SourceWeights({'meshes':meshes}).match([0,1,0],[.2,.3])

if __name__=='__main__': unittest.main()
