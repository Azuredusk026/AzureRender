import struct
import unittest
from endfield_native.lighting import decode_light_fields


class NativeLightTests(unittest.TestCase):
    def fields(self):
        # IDs and types from the pinned reader; game 1.0.14 has 46 fields.
        data = {0x11000000+i*65536:(4,struct.pack('<f',0)) for i in range(46)}
        for i in (0,1):data[0x11000000+i*65536]=(2,struct.pack('<i',2 if i==0 else 0))
        data[0x11020000]=(10,struct.pack('<4f',.8,.9,1,1))
        for i,v in ((3,20),(4,10),(5,60),(7,30),(21,6500)):data[0x11000000+i*65536]=(4,struct.pack('<f',v))
        for i in (22,26):data[0x11000000+i*65536]=(1,b'\0')
        return data

    def test_known_schema(self):
        value=decode_light_fields(self.fields())
        self.assertEqual(value['light_type'],2)
        self.assertEqual(value['intensity'],20)
        self.assertEqual(value['spot_angle'],60)

    def test_invalid_type(self):
        data=self.fields();data[0x11030000]=(3,struct.pack('<i',20))
        with self.assertRaises(ValueError):decode_light_fields(data)

    def test_invalid_values(self):
        for value in (float('nan'),-1):
            data=self.fields();data[0x11040000]=(4,struct.pack('<f',value))
            with self.assertRaises(ValueError):decode_light_fields(data)

    def test_unknown_schema(self):
        data=self.fields();data.pop(0x112d0000)
        with self.assertRaises(ValueError):decode_light_fields(data)


if __name__=='__main__':unittest.main()
