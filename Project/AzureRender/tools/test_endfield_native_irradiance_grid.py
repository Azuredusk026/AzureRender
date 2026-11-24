import json
from pathlib import Path
import struct
import tempfile
import unittest

from endfield_native import irradiance_grid
from test_endfield_native_irradiance import index, tile


class IrradianceGridTests(unittest.TestCase):
    def test_export_preserves_xyz_order_weights_and_source_selection(self):
        self.assertTrue(callable(getattr(irradiance_grid, 'export_grid', None)))
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root/'index.bytes').write_bytes(index())
            for name in ('iv_0_0.bytes', 'iv_123_0.bytes'): (root/name).write_bytes(tile())
            path=root/'field.aziv'
            report=irradiance_grid.export_grid(root,path,(0,0,0),.5,(2,2,2),1,[0])
            raw=path.read_bytes()
            self.assertEqual(len(raw),36+8*64)
            self.assertEqual(struct.unpack('<8s4f3I',raw[:36]),(b'AZIV0001',0.,0.,0.,.5,2,2,2))
            self.assertEqual(struct.unpack_from('<f',raw,36+48)[0],1)
            self.assertEqual(struct.unpack_from('<f',raw,36+64+48)[0],0)
            self.assertEqual(report['valid_samples'],1)
            self.assertEqual(report['tile_cache_misses'],1)
            self.assertEqual(report['file_index'],1)
            self.assertFalse(report['activation_verified'])
            self.assertEqual(json.loads(path.with_suffix('.json').read_text())['output_sha256'],report['output_sha256'])

    def test_export_rejects_invalid_geometry_and_fallback_order(self):
        self.assertTrue(callable(getattr(irradiance_grid, 'export_grid', None)))
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            for spacing,dims,lods in ((0,(2,2,2),[0]),(1,(0,2,2),[0]),(1,(2000,2000,2),[0]),
                                       (1,(2,2,2),[2,0]),(1,(2,2,2),[0,0])):
                with self.assertRaises(ValueError):irradiance_grid.export_grid(root,root/'x.aziv',(0,0,0),spacing,dims,0,lods)

    def test_explicit_lod_fallback_uses_first_occupied_source(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);raw=bytearray(index())
            offset=raw.rfind(struct.pack('<I',0x00404040))
            struct.pack_into('<I',raw,offset,0x01404040)
            (root/'index.bytes').write_bytes(raw)
            (root/'iv_123_0.bytes').write_bytes(tile())
            report=irradiance_grid.export_grid(root,root/'fallback.aziv',(0,0,0),2,(2,2,2),1,[0,1])
            self.assertEqual(report['samples_per_lod'],{'1':1})
            self.assertEqual(report['valid_samples'],1)
