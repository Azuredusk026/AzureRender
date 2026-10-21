"""Camera recovery must predict landmarks excluded from its fit."""
import copy
import unittest

try:
    from native_render.camera_calibration import calibrate
except ImportError:
    calibrate = None


class CameraCalibrationTests(unittest.TestCase):
    def fixture(self):
        # Independently specified pinhole: f=500px, center=(320,240).
        # Camera world position=(1,2,-3), forward=+Z, image down=+Y.
        pairs = [([-1,0,2],[120,40]),([3,0,2],[520,40]),
                 ([-1,4,2],[120,440]),([3,4,2],[520,440]),
                 ([-2,-1,7],[170,90]),([4,-1,7],[470,90]),
                 ([-2,5,7],[170,390]),([4,5,7],[470,390]),
                 ([1,2,5],[320,240]),([0,3,7],[270,290])]
        return dict(format='RenderCameraLandmarks/1',resolution=[640,480],
            maximum_error_pixels=1.,landmarks=[dict(id=str(i),world=w,pixel=p,
            role='fit' if i<8 else 'validation') for i,(w,p) in enumerate(pairs)])

    def test_recovers_metric_camera_and_predicts_held_out_landmarks(self):
        self.assertTrue(callable(calibrate), 'Landmark calibration is required')
        result=calibrate(self.fixture())
        for actual,expected in zip(result['camera']['position'],[1,2,-3]):
            self.assertAlmostEqual(actual,expected,places=7)
        self.assertAlmostEqual(result['intrinsics'][0][0],500,places=7)
        self.assertAlmostEqual(result['intrinsics'][1][1],500,places=7)
        self.assertLess(result['validation_maximum_error_pixels'],1e-7)
        self.assertTrue(result['projection_accepted'])
        self.assertFalse(result['reference_qualified'])

    def test_recovers_rotated_world_axes_without_changing_image_convention(self):
        d=self.fixture()
        for row in d['landmarks']:
            x,y,z=row['world']
            row['world']=[7-(z+3),-2+(y-2),1+(x-1)]
        result=calibrate(d)
        for actual,expected in zip(result['camera']['position'],[7,-2,1]):
            self.assertAlmostEqual(actual,expected,places=7)
        for actual,expected in zip(result['camera']['forward'],[-1,0,0]):
            self.assertAlmostEqual(actual,expected,places=7)
        self.assertTrue(result['projection_accepted'])

    def test_validation_points_cannot_hide_a_bad_correspondence(self):
        d=self.fixture();d['landmarks'][-1]['pixel'][0]+=20
        result=calibrate(d)
        self.assertAlmostEqual(result['validation_maximum_error_pixels'],20,places=6)
        self.assertFalse(result['projection_accepted'])
        self.assertAlmostEqual(result['intrinsics'][0][0],500,places=7)

    def test_planar_and_collinear_geometry_is_rejected(self):
        for planar in [True,False]:
            d=self.fixture()
            for row in d['landmarks']:
                row['world'][2]=2
                if not planar:row['world'][1]=0
            with self.assertRaisesRegex(ValueError,'degenerate'):
                calibrate(d)

    def test_rejects_missing_holdout_duplicate_identity_and_nonfinite_values(self):
        cases=[]
        d=self.fixture()
        for row in d['landmarks']:row['role']='fit'
        cases.append(d)
        d=self.fixture();d['landmarks'][1]['id']='0';cases.append(d)
        d=self.fixture();d['landmarks'][0]['world'][0]=float('nan');cases.append(d)
        d=self.fixture();d['resolution']=[640,False];cases.append(d)
        d=self.fixture();d['maximum_error_pixels']=True;cases.append(d)
        for d in cases:
            with self.assertRaises(ValueError):calibrate(copy.deepcopy(d))


if __name__=='__main__':unittest.main()
