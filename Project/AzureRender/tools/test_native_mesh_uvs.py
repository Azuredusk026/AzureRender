import unittest
import numpy as np

from endfield_native.mesh_uvs import match_uv_triangles
from endfield_native.mesh_corners import match_triangle_corners


class NativeMeshUvTests(unittest.TestCase):
    def setUp(self):
        self.positions = [[0,0,0],[1,0,0],[0,1,0],[0,0,0],[-1,0,0],[0,-1,0]]
        self.uv0 = [[0,0],[2,0],[0,2],[0,0],[-2,0],[0,-2]]
        self.uv1 = [[.1,.2],[.3,.2],[.1,.4],[.7,.8],[.5,.8],[.7,.6]]
        self.triangles = [[0,1,2],[3,4,5]]

    def run_match(self, order):
        return match_uv_triangles(self.positions, self.uv0, self.uv1, self.triangles,
            [self.positions[i] for i in order], [self.uv0[i] for i in order])

    def test_reordered_corners_preserve_independent_uv_and_seams(self):
        order = [5,4,3,0,2,1]
        result, audit = self.run_match(order)
        np.testing.assert_array_equal(result, np.array(self.uv1)[order])
        self.assertEqual(audit['corners'], 6)
        self.assertEqual(audit['maximumPositionError'], 0)

    def test_float_rounding_and_cell_boundaries_are_tolerated(self):
        corner = [[x+1e-6,y-1e-6,z] for x,y,z in self.positions]
        result, audit = match_uv_triangles(self.positions, self.uv0, self.uv1, self.triangles, corner, self.uv0)
        np.testing.assert_array_equal(result, self.uv1)
        self.assertLessEqual(audit['maximumPositionError'], 1.01e-6)

    def test_identical_source_duplicates_are_allowed(self):
        self.triangles.append(self.triangles[0])
        result, _ = self.run_match([0,1,2])
        np.testing.assert_array_equal(result, self.uv1[:3])

    def test_ambiguous_secondary_seam_is_an_error(self):
        with self.assertRaisesRegex(ValueError, 'ambiguous'):
            match_uv_triangles(self.positions[:3]*2,self.uv0[:3]*2,self.uv1,
                self.triangles,self.positions[:3],self.uv0[:3])

    def test_missing_source_corner_is_an_error(self):
        with self.assertRaisesRegex(ValueError, 'matching'):
            self.run_match([0,1,5])

    def test_malformed_or_nonfinite_streams_are_rejected(self):
        for positions, uv0, uv1 in [([],[],[]), (self.positions,self.uv0[:1],self.uv1),
                (self.positions,self.uv0,[[float('nan'),0]]*6),
                ([[0,0]]*6,self.uv0,self.uv1)]:
            with self.subTest(positions=positions), self.assertRaises(ValueError):
                match_uv_triangles(positions, uv0, uv1, self.triangles, self.positions[:3], self.uv0[:3])
        for triangles in ([[0,1,99]], [[0,1,-1]], [[0,1,1.5]], [[0,1]], []):
            with self.subTest(triangles=triangles), self.assertRaises(ValueError):
                match_uv_triangles(self.positions,self.uv0,self.uv1,triangles,self.positions[:3],self.uv0[:3])

    def test_same_primary_seam_selects_secondary_coordinates_by_face(self):
        result, _ = self.run_match([0,1,2,3,4,5])
        np.testing.assert_array_equal(result[[0,3]], [[.1,.2],[.7,.8]])

    def test_three_component_corner_attributes_preserve_normal_splits(self):
        values=np.array([[1,0,0]]*3+[[0,1,0]]*3)
        order=[5,4,3,0,2,1]
        result,_=match_triangle_corners(self.positions,self.uv0,values,self.triangles,
            np.array(self.positions)[order],np.array(self.uv0)[order])
        np.testing.assert_array_equal(result,values[order])

    def test_corner_attribute_shape_and_ambiguity_are_validated(self):
        for values in (np.empty((6,0)),[[0,1,2]],[[float('nan'),0,1]]*6):
            with self.assertRaises(ValueError):
                match_triangle_corners(self.positions,self.uv0,values,self.triangles,
                    self.positions[:3],self.uv0[:3])
        with self.assertRaisesRegex(ValueError,'ambiguous'):
            match_triangle_corners(self.positions[:3]*2,self.uv0[:3]*2,
                [[1,0,0]]*3+[[0,1,0]]*3,self.triangles,self.positions[:3],self.uv0[:3])

    def test_winding_distinguishes_coincident_front_and_back_normals(self):
        positions=self.positions[:3]*2;uv0=self.uv0[:3]*2;values=[[0,0,1]]*3+[[0,0,-1]]*3
        for orientation,expected in [('same',-1),('opposite',1)]:
            result,_=match_triangle_corners(positions,uv0,values,[[0,1,2],[3,5,4]],
                [positions[i] for i in [0,2,1]],[uv0[i] for i in [0,2,1]],orientation=orientation)
            np.testing.assert_array_equal(result,[[0,0,expected]]*3)
        with self.assertRaises(ValueError):
            match_triangle_corners(positions,uv0,values,[[0,1,2]],positions[:3],uv0[:3],orientation='unknown')


if __name__ == '__main__':
    unittest.main()
