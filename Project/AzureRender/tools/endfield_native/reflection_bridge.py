"""Static serialized probe parameters for the observed native CPU upload path.

Evidence: UnityPlayer SHA in reflection_semantics.py; property decoder
0x180148650, object converter 0x180e70b70, ECS producer 0x180417d30.
Matrices describe the serialized static snapshot. Native runtime transforms,
activation, registration identity, array residency and fade are separate.
"""
import math
import struct

from .reflection_semantics import pack_runtime_probe


def decode_serialized_probe(fields):
    def read(index, kind, fmt):
        key = hex(0x10000000 | (index << 16))
        value = fields.get(key)
        if value is None or value.get('type') != kind:
            raise ValueError(f'Missing or incorrectly typed reflection property {key}')
        try:
            raw = bytes.fromhex(value['hex'])
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError(f'Invalid reflection property bytes {key}') from error
        if len(raw) != struct.calcsize('<' + fmt):
            raise ValueError(f'Incorrect reflection property size {key}')
        result = list(struct.unpack('<' + fmt, raw))
        if not all(math.isfinite(x) for x in result):
            raise ValueError(f'Nonfinite reflection property {key}')
        return result
    mode = read(8, 2, 'i')[0]
    if mode not in (0, 2):
        raise ValueError(f'Only serialized baked/custom probe modes are supported: {mode}')
    importance_raw = read(9, 2, 'i')[0]
    signed_short = struct.unpack('<h', struct.pack('<H', importance_raw & 0xffff))[0]
    matrix_a = read(4, 17, '16f')
    return {
        'mode': mode,
        'half_extent': [abs(x * .5) for x in read(0, 9, '3f')],
        'influence': read(3, 9, '3f'),
        'matrix_a': matrix_a,
        'matrix_b': read(5, 17, '16f'),
        'matrix_source': 'serialized_static_snapshot',
        # Matrix A places the normalized influence box in Unity world space.
        'projection_origin': list(matrix_a[12:15]),
        'intensity': read(6, 4, 'f')[0],
        'box_projection': bool(read(7, 1, 'B')[0]),
        'importance': max(signed_short, 0),
        'shape_type': read(13, 2, 'i')[0],
        'sh_coefficients': read(10 if mode == 0 else 11, 17, '27f'),
        'sh_source_field': '0x100a0000' if mode == 0 else '0x100b0000',
        'activation': None,
        'registration_sort_key': None,
    }


def pack_serialized_probe(fields, *, array_layer, fade):
    """Pack confirmed parameters; caller supplies array layer and fade.

    The temporary record contains only CB-consumed fields. Its zero identity
    bytes are never exposed or used for sorting. Callers must resolve native
    registration identity separately when ordering probes of equal importance.
    """
    p = decode_serialized_probe(fields)
    data = bytearray(0x130)
    data[0] = (2 if p['box_projection'] else 0) | (4 if p['shape_type'] == 1 else 0)
    struct.pack_into('<i', data, 4, p['importance'])
    struct.pack_into('<3f', data, 8, *p['influence'])
    struct.pack_into('<f', data, 0x14, p['intensity'])
    struct.pack_into('<3f', data, 0x18, *p['half_extent'])
    struct.pack_into('<16f', data, 0x24, *p['matrix_a'])
    struct.pack_into('<16f', data, 0x64, *p['matrix_b'])
    struct.pack_into('<27f', data, 0xa4, *p['sh_coefficients'])
    return pack_runtime_probe(data, array_layer, fade)
