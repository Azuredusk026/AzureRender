"""Check source Unity shader program boundaries and binding names."""

import struct
import unittest

from tools.npr_room.unity_d3d11_programs import dxbc_entry, program_bindings


class D3D11ProgramTests(unittest.TestCase):
    def test_program_index_selects_one_complete_dxbc_container(self):
        program = b"DXBC" + bytes(16) + struct.pack("<III", 1, 44, 1) + struct.pack("<I", 36) + b"SHEX\0\0\0\0"
        header = struct.pack("<I3I3I", 2, 28, 6, 0, 34, 6 + len(program), 0)
        block = header + b"params" + b"prefix" + program
        self.assertEqual(dxbc_entry(block, 1), program)
        with self.assertRaisesRegex(ValueError, "DXBC"):
            dxbc_entry(block, 0)
        with self.assertRaisesRegex(ValueError, "range"):
            dxbc_entry(block[:-1], 1)

    def test_bindings_preserve_constant_buffer_offsets_and_resource_slots(self):
        parameters = {"m_ConstantBufferBindings": [{"m_NameIndex": 0, "m_Index": 1}],
                      "m_ConstantBuffers": [{"m_NameIndex": 0, "m_Size": 32,
                                             "m_VectorParams": [{"m_NameIndex": 1, "m_Index": 16, "m_Dim": 3,
                                                                "m_Type": 0, "m_ArraySize": 0}],
                                             "m_MatrixParams": []}],
                      "m_TextureParams": [{"m_NameIndex": 2, "m_Index": 4, "m_SamplerIndex": 6,
                                           "m_Dim": 2, "m_MultiSampled": False}]}
        names = {0: "$Globals", 1: "CZY_HorizonColor", 2: "CZY_StarMap"}
        bindings = program_bindings(parameters, names)
        cb, = bindings["constantBuffers"]
        self.assertEqual((cb["name"], cb["binding"], cb["bytes"]), ("$Globals", 1, 32))
        self.assertEqual(cb["vectors"][0]["offset"], 16)
        self.assertEqual(bindings["textures"][0], {"name": "CZY_StarMap", "binding": 4, "sampler": 6,
                                                   "dimension": 2, "multisampled": False})


if __name__ == "__main__":
    unittest.main()
