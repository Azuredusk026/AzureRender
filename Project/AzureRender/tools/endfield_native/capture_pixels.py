"""Read tightly packed raw replay texels with an explicit vertical convention."""
import math
import struct


def read_pixel(raw,*,pixel_format,width,height,x,y,flip_y):
    sizes={'R11G11B10_FLOAT':4,'R16G16B16A16_FLOAT':8,'R8G8B8A8_UNORM':4}
    if not isinstance(raw,(bytes,bytearray,memoryview)):raise ValueError('Captured pixels require a binary buffer')
    if not isinstance(pixel_format,str) or pixel_format not in sizes:
        raise ValueError('Unsupported captured pixel format')
    if any(type(v) is not int for v in (width,height,x,y)) or width<=0 or height<=0 or not 0<=x<width or not 0<=y<height:
        raise ValueError('Captured pixel requires positive dimensions and bounded integer coordinates')
    if type(flip_y) is not bool or len(raw)!=width*height*sizes[pixel_format]:
        raise ValueError('Captured pixels require an explicit origin and complete tightly packed bytes')
    offset=((height-1-y if flip_y else y)*width+x)*sizes[pixel_format]
    if pixel_format=='R11G11B10_FLOAT':
        word=struct.unpack_from('<I',raw,offset)[0]
        def channel(shift,bits):
            value=(word>>shift)&((1<<(bits+5))-1);exponent=value>>bits;mantissa=value&((1<<bits)-1)
            if exponent==31:raise ValueError('Captured pixel contains a nonfinite channel')
            return math.ldexp(mantissa,-14-bits) if exponent==0 else math.ldexp(1+mantissa/(1<<bits),exponent-15)
        result=(channel(0,6),channel(11,6),channel(22,5),1)
    elif pixel_format=='R16G16B16A16_FLOAT':
        result=struct.unpack_from('<4e',raw,offset)
    else:
        result=tuple(v/255 for v in struct.unpack_from('<4B',raw,offset))
    if not all(math.isfinite(v) for v in result):raise ValueError('Captured pixel contains a nonfinite channel')
    return result
