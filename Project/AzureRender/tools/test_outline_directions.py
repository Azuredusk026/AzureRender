import unittest
import numpy as np
from native_render.outline_directions import encode_octahedral,decode_octahedral,smooth_direction_coordinates


class OutlineDirectionTests(unittest.TestCase):
    def test_full_sphere_and_seams(self):
        directions=np.array([[0,0,1],[0,0,-1],[1,0,0],[-1,0,0],[0,1,0],[0,-1,0],
                             [1,2,-3],[-3,2,-1],[.01,-.2,.7]],dtype=float)
        expected=directions/np.linalg.norm(directions,axis=1,keepdims=True)
        encoded=encode_octahedral(directions)
        self.assertEqual(encoded.dtype,np.float32)
        self.assertLessEqual(np.abs(encoded).max(),1)
        np.testing.assert_allclose(decode_octahedral(encoded),expected,atol=2e-7)

    def test_nonorthogonal_source_preserves_negative_hemisphere(self):
        source_n=np.array([[0,0,1.02],[0,0,.98]])
        source_t=np.array([[1,0,-.1,1],[1,0,.2,-1]])
        uv=np.array([[1,0],[0,1]],dtype=float)
        imported_n=np.array([[0,0,1],[0,0,1]])
        imported_t=np.array([[1,0,0,1],[1,0,0,1]])
        converted=smooth_direction_coordinates(source_n,source_t,uv,imported_n,imported_t,np.eye(3))
        self.assertLess(converted[0,2],0)
        np.testing.assert_allclose(converted[0],[1/np.sqrt(1.01),0,-.1/np.sqrt(1.01)],atol=1e-12)
        np.testing.assert_allclose(converted[1],[0,-1,0],atol=1e-12)
        np.testing.assert_allclose(decode_octahedral(encode_octahedral(converted)),converted,atol=2e-7)

    def test_reflected_import_frame_uses_authored_handedness(self):
        n=np.array([[0,0,1]],dtype=float);t=np.array([[1,0,0,-1]],dtype=float)
        uv=np.array([[.3,.4]])
        converted=smooth_direction_coordinates(n,t,uv,n,np.array([[-1,0,0,-1]]),np.diag([-1,1,1]))
        np.testing.assert_allclose(converted,[[.3,-.4,np.sqrt(.75)]],atol=1e-12)

    def test_invalid_inputs_fail_before_encoding(self):
        for values in [[[0,0,0]],[[1,np.nan,0]],[[1,0]],[]]:
            with self.assertRaises(ValueError):encode_octahedral(values)
        with self.assertRaises(ValueError):decode_octahedral([[1.1,0]])
        n=np.array([[0,0,1]],dtype=float);t=np.array([[1,0,0,1]],dtype=float);uv=np.array([[0,0]],dtype=float)
        for bad_n,bad_t,bad_uv,transform in [
                (n,t,[[1.1,0]],np.eye(3)),(n,t,uv,np.diag([2,1,1])),
                (n,[[0,0,1,1]],uv,np.eye(3)),(n,[[1,0,0,0]],uv,np.eye(3))]:
            with self.assertRaises(ValueError):smooth_direction_coordinates(bad_n,bad_t,bad_uv,n,bad_t,transform)


if __name__=='__main__':unittest.main()
