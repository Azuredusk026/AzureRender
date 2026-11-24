"""Offline reference for the observed UnityPlayer reflection upload structure.

Inputs are 0x130-byte runtime records, NOT serialized FlatBuffer components.
The serialized-to-runtime bridge and scene activation remain unverified.
Results implement real-number arithmetic with decoded float32 constants;
they are not a bit-exact CPU SIMD emulator.
"""
import math
import struct


SOURCE_SHA256 = '9aa9397a6c5ee4fa083ea379b81ff14019eeba26a76d15446318d5947fb3d2d0'
UPLOAD_VA = 0x18021CDB0
COMPARE_VA = 0x180DC70E0
LUMINANCE_VA = 0x180DBE9D0
LUMINANCE = (0.2126729041337967, 0.7151522040367126, 0.07217500358819962)
INFLUENCE_LIMIT = struct.unpack('<f', bytes.fromhex('a4707d3f'))[0]


def _record(data):
    if not isinstance(data, (bytes, bytearray)) or len(data) != 0x130:
        raise ValueError('Expected one 0x130-byte native runtime reflection record')
    return data


def sort_runtime_probes(records):
    """Signed int at +4 descending; unsigned uint64 at +0x110 ascending.

    Exact-key ties retain Python input order; native introsort tie order is
    unspecified. The +4 field's semantic name is deliberately unassigned.
    """
    def key(data):
        _record(data)
        return (-struct.unpack_from('<i', data, 4)[0],
                struct.unpack_from('<Q', data, 0x110)[0])
    return sorted(records, key=key)


def pack_runtime_probe(data, array_layer, fade):
    """Return eight float4 rows for CB2[12 + 8*i .. 19 + 8*i].

    Array layer and fade are external runtime-state inputs. Layer zero is
    reserved for the default environment. This function does not select,
    activate, load or assign layers to scene probes.
    """
    _record(data)
    if type(array_layer) is not int or array_layer < 1 or array_layer > 2**24:
        raise ValueError('Local probe requires a positive exactly representable array layer')
    if not isinstance(fade, (int, float)) or not math.isfinite(fade) or not 0 <= fade <= 1:
        raise ValueError('Fade must be finite and between zero and one')
    def floats(offset, count):
        values = struct.unpack_from('<' + 'f' * count, data, offset)
        if not all(math.isfinite(x) for x in values):
            raise ValueError(f'Nonfinite runtime values at {offset:#x}')
        return values
    influence = floats(8, 3)
    intensity = floats(0x14, 1)[0]
    extent = floats(0x18, 3)
    if any(x <= 0 for x in extent):
        raise ValueError('Runtime half extents must be positive')
    matrix_a, matrix_b = floats(0x24, 16), floats(0x64, 16)
    sh = floats(0xa4, 27)
    sh_luminance = tuple(sum(sh[channel * 9 + coefficient] * LUMINANCE[channel]
                             for channel in range(3)) for coefficient in (3, 1, 2, 0))
    inverse_widths = tuple(1 / ((1 - min(value, INFLUENCE_LIMIT)) * size)
                           for value, size in zip(influence, extent))
    flags = data[0]
    rank = struct.unpack_from('<i', data, 4)[0]
    return [sh_luminance, (*extent, array_layer),
            *(tuple(matrix_b[column + 4 * row] for row in range(4)) for column in range(3)),
            (*inverse_widths, intensity),
            (int(bool(flags & 2)), int(not flags & 4), rank, fade),
            (*matrix_a[12:15], math.sqrt(sum(value * value for value in extent)))]
