import tempfile
import unittest
import struct
import subprocess
import sys
from pathlib import Path
import numpy as np
from native_render.cubemap_environment import convert

class CubeEnvironmentTests(unittest.TestCase):
    def test_cube_output_preserves_six_independent_authored_faces(self):
        # A final mip with six unrelated HDR colors must keep every face.
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            expected=bytearray()
            for mip in range(2):
                size=2>>mip
                for face in range(6):
                    pixels=np.tile(np.array([face+1, 10+mip, .25, 1],dtype='<f4'),(size,size,1))
                    pixels.tofile(root/f'face{face}_mip{mip}.rgba32f')
                    expected.extend(pixels.astype('<f2').tobytes())
            result=subprocess.run([sys.executable,str(Path(__file__).parent/'native_render/cubemap_environment.py'),
                '--source',str(root),'--output',str(root/'cube.azenv'),
                '--face-size','2','--mips','2','--projection','cube'],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            data=(root/'cube.azenv').read_bytes()
            self.assertEqual(data[:20],b'AZENV002'+struct.pack('<III',2,2,2))
            self.assertEqual(data[20:],bytes(expected))
            report=__import__('json').loads((root/'cube.azenv.json').read_text())
            self.assertEqual(report['projection'],'cube')
            self.assertEqual(len(report['inputs']),12)

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
