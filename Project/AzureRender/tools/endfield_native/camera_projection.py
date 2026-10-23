"""Decode caller-confirmed, column-major projection matrices for a downward raster Y axis."""
import math


def decode_raster_offset(projection,*,projection_kind):
    if projection_kind not in ('perspective','orthographic'):
        raise ValueError('An explicit projection kind is required')
    if len(projection)!=16 or any(type(v) not in (int,float) or not math.isfinite(v) for v in projection):
        raise ValueError('Projection requires sixteen finite numeric columns')
    if projection[0]<=0 or projection[5]>=0:
        raise ValueError('The confirmed rightward X and downward Y raster convention is required')
    if projection_kind=='perspective':
        if projection[11]!=-1 or projection[15]!=0:raise ValueError('Unsupported perspective clip-W convention')
        x,y=-projection[8],-projection[9]
    else:
        if projection[11]!=0 or projection[15]!=1:raise ValueError('Unsupported orthographic clip-W convention')
        x,y=projection[12],projection[13]
    if abs(x)>2 or abs(y)>2:raise ValueError('Projection offset exceeds the public camera domain')
    return dict(projectionOffsetX=x,projectionOffsetY=y)
