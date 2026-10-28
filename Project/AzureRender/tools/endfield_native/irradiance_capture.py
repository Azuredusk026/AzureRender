"""Decode and sample explicit native GPU volume payloads at mip zero.

Arrays use depth, height, width, channel order. World positions already include
any source normal offset or temporal jitter. Composition and runtime activation
remain separate consumers; this module never substitutes unobserved globals.
"""
import math
import numpy as np


def decode_volume(payload, pixel_format, dimensions):
    if (not isinstance(dimensions,(tuple,list)) or len(dimensions)!=3 or any(type(x) is not int or x<=0 for x in dimensions)
            or math.prod(dimensions)>(1<<26)):
        raise ValueError('Invalid captured volume dimensions')
    width,height,depth=dimensions
    if not isinstance(payload,(bytes,bytearray)) or len(payload)!=width*height*depth*4:
        raise ValueError('Captured volume payload does not match dimensions')
    if pixel_format=='R11G11B10_FLOAT':
        words=np.frombuffer(payload,dtype='<u4')
        bits=np.stack(((words&2047)<<4,(words>>7)&32752,(words>>17)&32736),axis=-1).astype('<u2')
        values=bits.view('<f2').astype(np.float32).reshape(depth,height,width,3)
    elif pixel_format in ('RGBA8_UNORM','R8G8B8A8_UNORM'):
        values=np.frombuffer(payload,dtype=np.uint8).astype(np.float32).reshape(depth,height,width,4)/255
    else:
        raise ValueError('Unsupported captured volume format')
    if not np.isfinite(values).all():raise ValueError('Nonfinite captured volume')
    values.flags.writeable=False
    return values


def sample_linear(volume, uvw, address):
    if (not isinstance(volume,np.ndarray) or volume.ndim!=4 or min(volume.shape)<=0 or volume.dtype.kind not in 'fiu'
            or address not in ('clamp','wrap')):
        raise ValueError('Invalid sampled volume or address mode')
    coordinates=np.asarray(uvw,dtype=float)
    if coordinates.shape!=(3,) or not np.isfinite(coordinates).all():
        raise ValueError('Invalid texture coordinates')
    size=np.array(volume.shape[2::-1],dtype=int)
    coordinates=np.mod(coordinates,1) if address=='wrap' else np.clip(coordinates,0,1)
    texel=coordinates*size-.5
    base=np.floor(texel).astype(int);fraction=texel-base
    output=np.zeros(volume.shape[3],dtype=float)
    for z in (0,1):
        for y in (0,1):
            for x in (0,1):
                corner=np.array([x,y,z]);index=base+corner
                index=np.mod(index,size) if address=='wrap' else np.clip(index,0,size-1)
                value=volume[index[2],index[1],index[0]]
                if not np.isfinite(value).all():raise ValueError('Nonfinite sampled texel')
                weight=np.prod(np.where(corner,fraction,1-fraction))
                output+=value*weight
    return output


def sample_layer(world_position, spacing, amplitude, directional):
    if type(spacing) not in (int,float) or not math.isfinite(spacing) or spacing<=0:
        raise ValueError('Invalid irradiance layer spacing')
    if (not isinstance(amplitude,np.ndarray) or amplitude.ndim!=4 or amplitude.shape[3]!=3
            or not isinstance(directional,np.ndarray) or directional.ndim!=4
            or directional.shape!=(amplitude.shape[0],3*amplitude.shape[1],amplitude.shape[2],4)):
        raise ValueError('Irradiance textures require amplitude RGB and three stacked RGBA groups')
    position=np.asarray(world_position,dtype=float)
    if position.shape!=(3,) or not np.isfinite(position).all():raise ValueError('Invalid world position')
    dimensions=np.array(amplitude.shape[2::-1],dtype=float)
    uvw=np.mod((position/spacing+.5)/dimensions,1)
    rgb=sample_linear(amplitude,uvw,'wrap')
    # Clamp within a single color block before dividing Y by three. This
    # preserves the native half-texel border and prevents inter-color blending.
    uvw[1]=np.clip(uvw[1],.5/dimensions[1],1-.5/dimensions[1])/3
    groups=[]
    for color in range(3):
        coordinate=uvw.copy();coordinate[1]+=color/3
        groups.append(sample_linear(directional,coordinate,'clamp').tolist())
    return dict(amplitude=rgb.tolist(),directionalRgba=groups)
