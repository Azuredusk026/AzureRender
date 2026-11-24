"""Validate explicit captured projection columns and public camera offsets."""
import math
import unittest
from endfield_native import camera_projection as subject


class CameraProjectionTests(unittest.TestCase):
    def perspective(self):
        return [1.784,0,0,0,0,-3.1716,0,0,-1/5120,-1/4320,.00001,-1,0,0,.100001,0]

    def test_perspective_offset_uses_clip_w_and_raster_axis_convention(self):
        result=subject.decode_raster_offset(self.perspective(),projection_kind='perspective')
        self.assertEqual(result,dict(projectionOffsetX=1/5120,projectionOffsetY=1/4320))
        self.assertAlmostEqual(result['projectionOffsetX']*2560/2,.25)
        self.assertAlmostEqual(result['projectionOffsetY']*1440/2,1/6)

    def test_orthographic_offset_uses_translation(self):
        matrix=[1,0,0,0,0,-1,0,0,0,0,-.2,0,.25,-.5,0,1]
        self.assertEqual(subject.decode_raster_offset(matrix,projection_kind='orthographic'),dict(projectionOffsetX=.25,projectionOffsetY=-.5))

    def test_invalid_matrices_and_unconfirmed_axis_layout_are_rejected(self):
        cases=[self.perspective()[:-1]]
        for i,value in ((0,-1),(5,3),(8,3),(9,float('nan')),(11,1),(15,1),(0,True)):
            matrix=self.perspective();matrix[i]=value;cases.append(matrix)
        for matrix in cases:
            with self.assertRaises(ValueError):subject.decode_raster_offset(matrix,projection_kind='perspective')
        with self.assertRaises(ValueError):subject.decode_raster_offset(self.perspective(),projection_kind='unknown')

    def captured_inverse_view(self):
        return [
            .999987781047821,2.915582358875568e-11,.004935546778142452,0,
            .00029462671955116093,.9982166290283203,-.05969411879777908,0,
            .004926745314151049,-.059694841504096985,-.9982044696807861,0,
            .26604312658309937,2.262058973312378,-24.905324935913086,1]

    def test_captured_pose_requires_coherent_world_handedness(self):
        inverse=self.captured_inverse_view();projection=self.perspective()
        for signs in [(-1,1,1),(1,1,-1)]:
            basis=[signs[0],0,0,0,signs[1],0,0,0,signs[2]]
            result=subject.decode_raster_camera(inverse,projection,world_to_engine=basis)
            self.assertEqual(result['position'],[signs[i]*inverse[12+i] for i in range(3)])
            for i in range(3):
                self.assertAlmostEqual(result['target'][i]-result['position'][i],-signs[i]*inverse[8+i])
                self.assertAlmostEqual(result['up'][i],signs[i]*inverse[4+i])
            x,y,z=map(math.radians,result['rotationDegrees'])
            x,z=-x,-z # Engine LinearMath uses negative X and Z rotation angles.
            cx,sx,cy,sy,cz,sz=math.cos(x),math.sin(x),math.cos(y),math.sin(y),math.cos(z),math.sin(z)
            rows=[[cy*cz,-cy*sz,sy],[sx*sy*cz+cx*sz,-sx*sy*sz+cx*cz,-sx*cy],
                  [-cx*sy*cz+sx*sz,cx*sy*sz+sx*cz,cx*cy]]
            for row in range(3):
                for col in range(3):self.assertAlmostEqual(rows[row][col],signs[row]*inverse[4*col+row],places=6)
            for point in [(0,1.04,-2.8),(-5,1.04,0),(5,1.04,0),(0,8,0)]:
                native=[sum(inverse[4*c+i]*(point[i]-inverse[12+i]) for i in range(3)) for c in range(3)]
                mapped=[signs[i]*point[i]-result['position'][i] for i in range(3)]
                engine=[sum(rows[i][c]*mapped[i] for i in range(3)) for c in range(3)]
                for c in range(3):self.assertAlmostEqual(engine[c],native[c],places=5)
            self.assertAlmostEqual(result['fieldOfView'],math.degrees(2*math.atan(1/3.1716)))
            self.assertAlmostEqual(result['aspect'],3.1716/1.784)
        with self.assertRaises(ValueError):subject.decode_raster_camera(inverse,projection,world_to_engine=[1,0,0,0,1,0,0,0,1])

    def test_camera_pose_rejects_nonrigid_or_invalid_basis(self):
        inverse=self.captured_inverse_view();projection=self.perspective();basis=[-1,0,0,0,1,0,0,0,1]
        for index,value in [(0,2),(3,.1),(4,float('nan')),(15,0)]:
            bad=inverse.copy();bad[index]=value
            with self.assertRaises(ValueError):subject.decode_raster_camera(bad,projection,world_to_engine=basis)
        for bad in [basis[:-1],[2,0,0,0,1,0,0,0,1],[False,0,0,0,1,0,0,0,1]]:
            with self.assertRaises(ValueError):subject.decode_raster_camera(inverse,projection,world_to_engine=bad)
        skew=projection.copy();skew[4]=.2
        with self.assertRaises(ValueError):subject.decode_raster_camera(inverse,skew,world_to_engine=basis)

    def test_camera_pose_supports_both_gimbal_limits(self):
        for sign in [-1,1]:
            inverse=[0,0,-sign,0,0,1,0,0,sign,0,0,0,2,3,4,1]
            result=subject.decode_raster_camera(inverse,self.perspective(),world_to_engine=[1,0,0,0,1,0,0,0,1])
            self.assertEqual(result['rotationDegrees'],[0,sign*90,0])
            self.assertEqual(result['position'],[2,3,4])


if __name__=='__main__':unittest.main()
