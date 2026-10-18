import tempfile
import unittest
from pathlib import Path
import numpy as np
from native_render.cubemap_environment import convert

class CubeEnvironmentTests(unittest.TestCase):
    def test_authored_levels_and_engine_direction_convention(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            for face in range(6):
                for mip in range(2):
                    size=2>>mip
                    color=[face+1, 10+mip, .25, 1]
                    np.tile(np.array(color,dtype='<f4'),(size,size,1)).tofile(root/f'face{face}_mip{mip}.rgba32f')
            report=convert(root,root/'test.azenv',2,2)
            data=(root/'test.azenv').read_bytes()
            self.assertEqual(data[:8],b'AZENV001')
            self.assertEqual(report['mips'],2)
            image=np.frombuffer(data[20:],dtype='<f2')[:8*4*4].reshape(4,8,4)
            self.assertTrue(np.all(image[:,:,1]==10))
            coarse=np.frombuffer(data[20+8*4*8:],dtype='<f2').reshape(2,4,4)
            self.assertTrue(np.all(coarse[:,:,1]==11))
            # Longitude 0 looks toward +X, longitude 1/4 toward -Z.
            self.assertEqual(image[2,4,0],1)
            self.assertEqual(image[2,2,0],6)

if __name__=='__main__':unittest.main()
