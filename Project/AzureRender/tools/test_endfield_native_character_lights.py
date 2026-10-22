"""Validate original additional-light bytes and receiver conversion."""
import math
import struct
import unittest
from endfield_native import lighting_ecs


class CharacterLightTests(unittest.TestCase):
    def decode(self,mode=0,only=0,parameters=(1,1,0,0)):
        raw=struct.pack('<4fiB3xff',*parameters,mode,only,.5,-1)
        return lighting_ecs.decode_character_light_data(raw)

    def test_original_bytes_and_receivers(self):
        data=self.decode()
        self.assertEqual(data,dict(npr_parameters=[1,1,0,0],npr_type=0,character_only=False,
            volumetric_scattering_intensity=.5,falloff_exponent=-1))
        self.assertEqual(lighting_ecs.render_light_receivers(data),dict(schemaVersion=1,surfaces=True,characters=True))

    def test_ignore_character_and_character_only(self):
        for mode,only,surfaces,characters in [(16,0,True,False),(0,1,False,True),(16,1,False,False)]:
            self.assertEqual(lighting_ecs.render_light_receivers(self.decode(mode,only)),
                dict(schemaVersion=1,surfaces=surfaces,characters=characters))

    def test_mode_identity_preserved(self):
        for mode in [0,1,2,3,4,16]:self.assertEqual(self.decode(mode)['npr_type'],mode)

    def test_strided_memory_view_preserves_source_bytes(self):
        raw=bytearray(64);raw[::2]=struct.pack('<4fiB3xff',1,1,0,0,16,0,.5,-1)
        self.assertEqual(lighting_ecs.decode_character_light_data(memoryview(raw)[::2])['npr_type'],16)

    def test_invalid_input_rejected(self):
        for raw in [b'',bytes(31),bytes(33),None]:
            with self.assertRaises(ValueError):lighting_ecs.decode_character_light_data(raw)
        for mode in [-1,5,15,17]:
            with self.assertRaises(ValueError):self.decode(mode)
        with self.assertRaises(ValueError):self.decode(only=2)
        for value in [math.nan,math.inf,-math.inf]:
            with self.assertRaises(ValueError):self.decode(parameters=(value,1,0,0))


if __name__=='__main__':unittest.main()
