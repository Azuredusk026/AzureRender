"""Decode the caller-confirmed CB3 light layout without reinterpreting mode bits as floats.

Event identity, active-index selection and coordinate origin belong to the caller.
Results retain raw records and list required runtime consumers independently from
generic emitter geometry. A decoded emitter is not a claim of shader equivalence.
"""
import math
import struct


def _finite(value,minimum,maximum,name):
    if type(value) not in (int,float) or not math.isfinite(value) or not minimum<=value<=maximum:
        raise ValueError('Invalid captured light '+name)
    return value


def _direction(encoded):
    x,y=[_finite(v,-1,1,'encoded axis') for v in encoded]
    z=.5*y+.5-abs(x)
    a=y-z
    b=math.copysign(max(1-abs(z)-abs(a),.00048828125),x)
    vector=[-z,a,b]  # Source zxy swizzle, then reflected world X.
    length=math.sqrt(sum(v*v for v in vector))
    if length<1e-8:raise ValueError('Invalid captured emitter axis')
    return [v/length for v in vector]


def decode_records(raw,indices,*,origin):
    if len(raw)!=(6+256*8)*16:raise ValueError('Captured light constant block must contain 2054 rows')
    if len(origin)!=3 or any(type(v) not in (int,float) or not math.isfinite(v) for v in origin):
        raise ValueError('Captured light origin must have three finite coordinates')
    indices=list(indices)
    if len(indices)>256 or any(type(i) is not int or not 0<=i<256 for i in indices) or len(set(indices))!=len(indices):
        raise ValueError('Invalid or duplicate captured light indices')
    result=[]
    for index in indices:
        offset=(6+index*8)*16;record=bytes(raw[offset:offset+128])
        rows=[struct.unpack_from('<4f',record,i*16) for i in range(8)]
        word=lambda row,column:struct.unpack_from('<I',record,row*16+column*4)[0]
        mode=word(3,3)
        if mode not in (0,1,2,3,4,16):raise ValueError('Unsupported captured character light mode')
        kind=rows[0][3]
        if kind not in (0,1):raise ValueError('Unsupported captured emitter kind')
        color=[_finite(c,0,65536,'color') for c in rows[0][:3]]
        source_position=[_finite(v,-1e9,1e9,'position') for v in rows[1][:3]]
        position=[v-o for v,o in zip(source_position,origin)];position[0]*=-1
        inverse_range=_finite(rows[1][3],1e-8,1e6,'inverse range')
        exponent=_finite(rows[6][3],-64,64,'falloff exponent')
        if exponent==0:raise ValueError('Captured falloff exponent must be nonzero')
        shape=dict(type='point' if kind else 'spot',direction=_direction(rows[2][:2]),
            falloff='smooth-inverse-square' if exponent<0 else 'range-power',falloffExponent=2 if exponent<0 else exponent)
        if not kind:
            cosine=_finite(rows[2][2],0,1,'outer cone cosine');scale=_finite(rows[2][3],1e-8,1e9,'cone scale')
            inner=cosine+1/scale
            if inner>1+1e-6:raise ValueError('Captured inner cone cosine exceeds one')
            shape.update(outerCone=math.acos(cosine),innerCone=math.acos(min(inner,1)))
        length=_finite(max(rows[2][2],0) if kind else 0,0,65536,'linear emitter length')
        parameters=[_finite(v,0,65536,'character parameters') for v in rows[4]]
        response=dict(parameters=parameters)
        if mode==0:
            response.update(diffuseFloor=_finite(parameters[0],0,4,'diffuse wrap')*.25,
                compression=_finite(parameters[1],0,1,'energy compression'))
        specular=_finite(rows[7][2],0,65536,'specular scale');shape['specularScale']=specular
        cookie=_finite(rows[7][3],-1,65535,'cookie index')
        if int(cookie)!=cookie:raise ValueError('Captured cookie index must be integral')
        flags=_finite(rows[5][3],0,3,'flags')
        if int(flags)!=flags:raise ValueError('Captured light flags must be integral')
        box={'enabled':bool(int(flags)&1)}
        if box['enabled']:
            packed=record[5*16:5*16+12]+record[6*16:6*16+12]
            half=struct.unpack('<12e',packed);matrix=[]
            for r in range(3):
                axis=half[r*4:r*4+3];translation=half[r*4+3]+sum(a*(o-p) for a,o,p in zip(axis,origin,source_position))
                matrix.append([-axis[0],axis[1],axis[2],translation])
            if not all(math.isfinite(v) for row in matrix for v in row):raise ValueError('Nonfinite captured influence box')
            determinant=sum(matrix[0][i]*(matrix[1][(i+1)%3]*matrix[2][(i+2)%3]-matrix[1][(i+2)%3]*matrix[2][(i+1)%3]) for i in range(3))
            if abs(determinant)<1e-12:raise ValueError('Singular captured influence box')
            threshold=_finite(rows[7][0],0,1,'box threshold')
            box.update(worldToBox=matrix,falloffThreshold=threshold)
        shape['influenceBox']=box
        capabilities=['wrapped-compressed-character-light'] if mode==0 else [
            'ordered-light-replace' if mode==4 else 'screen-depth' if mode==3 else 'character-mode-'+str(mode)]
        if length:capabilities.append('linear-emitter')
        if cookie>=0:capabilities.append('light-cookie')
        result.append(dict(sourceIndex=index,mode=mode,composition='replace' if mode==4 else 'add',
            characters=mode!=16,color=color,intensity=1,position=position,radius=1/inverse_range,shape=shape,
            linearLength=length,response=response,cookieIndex=int(cookie),flags=int(flags),
            rawShadowWords=[word(2,3),word(3,0)],requiredCapabilities=capabilities,rawRecordHex=record.hex()))
    return result


def default_character_light_color(color,attenuation,normal_dot_light,*,diffuse_floor,compression,scene_visibility):
    """Captured default mode: unattenuated color for diffuse and primary KK consumers."""
    for value,name in ((attenuation,'attenuation'),(diffuse_floor,'diffuse floor'),(compression,'compression'),(scene_visibility,'visibility')):
        _finite(value,0,1,name)
    _finite(normal_dot_light,-1,1,'normal cosine')
    if len(color)!=3:raise ValueError('Captured light color requires three values')
    color=[_finite(v,0,65536,'color') for v in color]
    denominator=max(max(color)*attenuation*(.5+.25*scene_visibility),1)
    energy=1-compression+compression/denominator
    wrapped=diffuse_floor+min(max(normal_dot_light+.5,0),1)*(1-diffuse_floor)
    return [value*energy*wrapped for value in color]


def line_emitter_response(incoming,axis,length):
    """Finite line geometry from the captured emitter branch, with defined axial fallbacks."""
    if len(incoming)!=3 or len(axis)!=3:raise ValueError('Line vectors require three values')
    incoming=[_finite(v,-1e9,1e9,'line incoming') for v in incoming]
    axis=[_finite(v,-1,1,'line axis') for v in axis];_finite(length,0,65536,'line length')
    if abs(sum(v*v for v in axis)-1)>1e-4:raise ValueError('Line axis must be normalized')
    dot=lambda a,b:sum(x*y for x,y in zip(a,b))
    unit=lambda v,fallback:[x/math.sqrt(dot(v,v)) for x in v] if dot(v,v)>1e-12 else fallback
    centre=unit(incoming,[0,0,1])
    if length==0:return dict(direction=centre,factor=1/(1+dot(incoming,incoming)))
    projection=dot(incoming,axis)
    direction=unit([v-a*projection for v,a in zip(incoming,axis)],centre)
    a=[v-x*length*.5 for v,x in zip(incoming,axis)];b=[v+x*length*.5 for v,x in zip(incoming,axis)]
    length_a=max(math.sqrt(dot(a,a)),1e-6);length_b=max(math.sqrt(dot(b,b)),1e-6)
    numerator=min(max(.5*(dot(direction,a)/length_a+dot(direction,b)/length_b),0),1)
    denominator=max(.5*(length_a*length_b+dot(a,b))+1,1)
    return dict(direction=direction,factor=numerator/denominator)


def color_blend_response(incoming,axis,radius,*,exponent,strength,facing_weight,normal,projected):
    """Literal ordered color mode, including its axial spot distance option."""
    for vector in (incoming,axis,normal):
        if len(vector)!=3:raise ValueError('Color blend vectors require three coordinates')
        for value in vector:_finite(value,-1e9,1e9,'blend vector')
    dot=lambda a,b:sum(x*y for x,y in zip(a,b))
    if abs(dot(axis,axis)-1)>1e-4 or abs(dot(normal,normal)-1)>1e-4:raise ValueError('Blend axes must be normalized')
    _finite(radius,1e-8,1e9,'blend radius');_finite(exponent,.1,64,'blend exponent')
    _finite(strength,0,1,'blend strength');_finite(facing_weight,0,1,'blend facing')
    if type(projected) is not bool:raise ValueError('Blend projection requires a boolean')
    incoming=[x*dot(incoming,axis) for x in axis] if projected else incoming
    squared=dot(incoming,incoming);length=math.sqrt(squared)
    direction=[x/length for x in incoming] if length>1e-6 else [0,0,0]
    attenuation=max(1-min(squared/(radius*radius),1),0)**exponent
    facing=min(max(dot(normal,direction)+.5,0),1);smooth=facing*facing*(3-2*facing)
    return dict(direction=direction,weight=attenuation*strength*(1-facing_weight+facing_weight*smooth))


def render_color_blend(record):
    """Convert a confirmed replacement mode to the public ordered blend shape."""
    if not isinstance(record,dict) or record.get('mode')!=4 or record.get('linearLength',0)!=0:
        raise ValueError('A confirmed punctual color replacement is required')
    import copy
    shape=copy.deepcopy(record['shape']);parameters=record['response']['parameters']
    strength=_finite(parameters[0],0,1,'blend strength');facing=_finite(parameters[3],0,1,'blend facing')
    exponent=max(2*parameters[1],.1);_finite(exponent,.1,64,'blend exponent')
    order=record['sourceIndex']
    if type(order) is not int or not 0<=order<=65535:raise ValueError('Invalid blend order')
    shape.update(falloff='range-power',falloffExponent=exponent,characterResponse=dict(schemaVersion=3,
        composition='color-blend',strength=strength,facingWeight=facing,
        projectedDistance=shape['type']=='spot' and parameters[2]>.5,order=order))
    return shape


def screen_rim_response(color,attenuation,base,normal,light,camera_axis,radial,depth,surface_depth,*,surface_tint):
    """Captured opaque screen rim: linear depth gate and projected local-light direction."""
    for vector in (color,base,normal,light,camera_axis,radial):
        if len(vector)!=3:raise ValueError('Screen rim vectors require three coordinates')
        for value in vector:_finite(value,-65536,65536,'screen rim vector')
    for value,name in ((attenuation,'rim attenuation'),(surface_tint,'rim tint')):_finite(value,0,1,name)
    for value in (depth,surface_depth):_finite(value,0,1e9,'linear depth')
    dot=lambda a,b:sum(x*y for x,y in zip(a,b))
    for vector in (normal,light,camera_axis,radial):
        if abs(dot(vector,vector)-1)>1e-4:raise ValueError('Screen rim directions must be normalized')
    planar=[a-b*dot(light,camera_axis) for a,b in zip(light,camera_axis)];length=math.sqrt(dot(planar,planar))
    planar=[v/length for v in planar] if length>1e-6 else [0,0,0]
    t=min(max((depth-surface_depth-.1)*10,0),1);window=t*t*(3-2*t)
    visibility=min(max(dot(radial,light)+1,0),1);facing=min(max(dot(normal,planar),0),1)
    return [c*attenuation*window*visibility*facing*(.5+surface_tint*(b*.96-.5)) for c,b in zip(color,base)]


def render_special_character_response(record):
    """Select only confirmed screen-rim or specular-only source modes."""
    if not isinstance(record,dict) or record.get('mode') not in (2,3) or record.get('linearLength',0)!=0:
        raise ValueError('A confirmed punctual special character light is required')
    if record['mode']==2:return dict(schemaVersion=4,composition='specular-only')
    words=record.get('rawShadowWords')
    if not isinstance(words,list) or len(words)!=2:raise ValueError('Screen rim requires explicit shadow selection words')
    planar=record['shape']['type']=='spot' or record.get('flags',0)&2
    if planar:
        index=struct.unpack('<f',struct.pack('<I',words[1]))[0]
        if not math.isfinite(index) or int(index)>=0:raise ValueError('Screen rim shadow atlas requires an independent consumer')
    elif any(((word>>shift)&1023)!=1023 for word in words for shift in (0,10,20)):
        raise ValueError('Screen rim cube-shadow atlas requires an independent consumer')
    p=record['response']['parameters']
    return dict(schemaVersion=4,composition='screen-rim',offsetScale=_finite(p[0],0,16,'rim offset'),surfaceTint=_finite(p[1],0,1,'rim tint'))


def render_material_rim_response(record):
    """Preserve an authored rim width for explicit material-selected consumers."""
    if not isinstance(record,dict) or record.get('mode')!=3:raise ValueError('Material rim requires a confirmed rim record')
    response=render_special_character_response(record)
    return dict(schemaVersion=5,composition='material-rim',width=response['offsetScale'],surfaceTint=response['surfaceTint'])


def render_character_response(record):
    """Map a decoded default-mode record to the supported public color-response version."""
    if not isinstance(record,dict) or record.get('mode')!=0 or not isinstance(record.get('response'),dict):
        raise ValueError('Captured character mode requires a separate runtime consumer')
    response=record['response']
    floor=_finite(response.get('diffuseFloor'),0,1,'diffuse floor')
    compression=_finite(response.get('compression'),0,1,'compression')
    length=_finite(record.get('linearLength'),0,65536,'linear length')
    result=dict(schemaVersion=2 if length else 1,diffuseFloor=floor,compression=compression)
    if length:result['linearLength']=length
    return result
