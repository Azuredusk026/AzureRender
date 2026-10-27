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


def decode_raster_camera(inverse_view,projection,*,world_to_engine):
    """Decode a captured pose with a caller-owned orthogonal world mapping.

    Matrices use column-major storage. The same mapping must transform scene
    positions, directions and spatial lighting inputs in the caller's recipe.
    The resulting camera basis must be right-handed and use local minus Z.
    """
    def finite(values,count):
        return len(values)==count and all(type(v) in (int,float) and math.isfinite(v) for v in values)
    def dot(a,b):return sum(x*y for x,y in zip(a,b))
    def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
    def rigid(columns):
        return all(abs(dot(columns[i],columns[j])-(i==j))<=1e-4 for i in range(3) for j in range(3))
    if not finite(inverse_view,16) or not finite(world_to_engine,9):
        raise ValueError('Camera matrices require finite numeric columns')
    if any(abs(inverse_view[i])>1e-6 for i in (3,7,11)) or abs(inverse_view[15]-1)>1e-6:
        raise ValueError('Inverse view requires an affine pose')
    mapping=[world_to_engine[i:i+3] for i in (0,3,6)]
    original=[inverse_view[i:i+3] for i in (0,4,8)]
    if not rigid(mapping) or not rigid(original):raise ValueError('Camera and world mapping must be orthogonal unit bases')
    def transform(vector):return [sum(mapping[c][r]*vector[c] for c in range(3)) for r in range(3)]
    columns=[transform(v) for v in original]
    if dot(cross(columns[0],columns[1]),columns[2])<1-1e-4:
        raise ValueError('World mapping must produce a right-handed engine camera')
    offsets=decode_raster_offset(projection,projection_kind='perspective')
    if any(abs(projection[i])>1e-6 for i in (1,2,3,4,6,7,12,13)):
        raise ValueError('Camera lens requires an axis-aligned perspective projection')
    # Engine scene transforms compose Rx(-x) * Ry(y) * Rz(-z).
    y=math.asin(max(-1,min(1,columns[2][0])))
    if abs(math.cos(y))>1e-6:
        x=math.atan2(-columns[2][1],columns[2][2]);z=math.atan2(-columns[1][0],columns[0][0])
    else:
        x=math.atan2(math.copysign(1,y)*columns[0][1],columns[1][1]);z=0
    position=transform(inverse_view[12:15])
    return dict(position=position,target=[position[i]-columns[2][i] for i in range(3)],up=columns[1],
                rotationDegrees=[math.degrees(v) for v in (-x,y,-z)],
                fieldOfView=math.degrees(2*math.atan(1/-projection[5])),
                aspect=-projection[5]/projection[0],**offsets)
