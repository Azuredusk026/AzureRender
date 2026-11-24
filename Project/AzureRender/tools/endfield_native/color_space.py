"""Native authored-color conversion with float32 operation boundaries."""
import math
import struct


def _f32(value):
    if type(value) not in (int,float) or not math.isfinite(value) or not 0<=value<=3.4028234663852886e38:
        raise ValueError('Native color requires a finite nonnegative float32 value')
    return struct.unpack('<f',struct.pack('<f',value))[0]


def srgb_to_linear_f32(value, *, hdr=False):
    if type(hdr) is not bool:raise ValueError('Native HDR color policy must be explicit')
    value=_f32(value)
    if value>1 and not hdr:raise ValueError('Authored sRGB color exceeds one')
    try:
        if value<=_f32(.04045):return _f32(value/_f32(12.92))
        if value<=1:return _f32(math.pow(_f32(_f32(value+_f32(.055))/_f32(1.055)),_f32(2.4)))
        return _f32(math.pow(value,_f32(2.2)))
    except (OverflowError,struct.error) as error:
        raise ValueError('Native HDR color exceeds float32 storage') from error
