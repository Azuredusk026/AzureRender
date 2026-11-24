"""Select a public surface model from an audited native VFX identity."""
import math
from .color_space import srgb_to_linear_f32


_UNSUPPORTED_LAYER_FEATURES = (
    '_UseInkWash','_UseDissolve','_UseDisturb','_UseFog','_UseNearCameraFade','_UseFresnel',
    '_UseAnimBreathing','_UseBright','_UseEdgeColor','_UsePolarUV','_UseScreenUV',
    '_UsePosYAsScreenV','_UseVertexOffset','_UseVertexOffsetMask','_UseTrail','_UseVATColorTex',
    '_HoudiniVATInParticle','_UseVFXPortal','_UseWaterBlend','_UseParticleDisturb','_UseWeightTex')


def _number(value, name, minimum=-3.4028234663852886e38, maximum=3.4028234663852886e38):
    if type(value) not in (int,float) or not math.isfinite(value) or not minimum<=value<=maximum:
        raise ValueError('Invalid native VFX '+name)
    return value


def _vector(value, count, name):
    if not isinstance(value,list) or len(value)!=count:raise ValueError('Invalid native VFX '+name)
    return [_number(v,name) for v in value]


def active_vfx_layers(record):
    """Resolve material inputs only; vertex and instance inputs remain separate."""
    unlit_vfx_profile(record)
    f,c=record.get('Floats',{}),record.get('Colors',{})
    for name in _UNSUPPORTED_LAYER_FEATURES:
        if _number(f.get(name),name)!=0:raise ValueError('Unsupported active native VFX '+name)
    for name,expected in [('_UseSoftBlend',1),('_SrcBlend',1),('_DstBlend',10),('_AlphaSrcBlend',1),
                          ('_AlphaDstBlend',10),('_BlendMode',0),('_ZWrite',0),('_CullMode',0)]:
        if _number(f.get(name),name)!=expected:raise ValueError('Unsupported native VFX '+name)
    active=['_MainTex'];keywords={'_USE_SOFTBLEND'}
    for index in range(6):
        flag='_UseSampleTex'+str(index);value=_number(f.get(flag),flag,0,1)
        if value not in (0,1):raise ValueError('Invalid native VFX '+flag)
        if value:active.append('_SampleTex'+str(index));keywords.add('_SAMPLE_TEX'+str(index))
    if len(active)>2:raise ValueError('Native VFX exceeds two active layers')
    state=record.get('RenderState',{});actual=state.get('ValidKeywords')
    if not isinstance(actual,list) or any(not isinstance(k,str) for k in actual) or len(actual)!=len(set(actual)) or set(actual)!=keywords:
        raise ValueError('Unsupported native VFX keywords')
    if _number(state.get('CustomRenderQueue'),'render queue')!=3000:raise ValueError('Unsupported native VFX render queue')
    textures={}
    for slot in record.get('Textures',[]):
        name=slot.get('Property')
        if not isinstance(name,str) or name in textures:raise ValueError('Invalid native VFX texture ownership')
        textures[name]=slot
    result=[]
    for name in active:
        main=name=='_MainTex';role='base';st_name=name
        if not main:
            role_index=_number(f.get(name+'Use'),name+'Use',0,5)
            if role_index not in (3,4):raise ValueError('Unsupported native VFX sample role')
            weights=[_number(f.get(name+'UseWeight'+str(i),0),name+'UseWeight',0,1) for i in range(6)]
            if weights!=[int(i==role_index) for i in range(6)]:raise ValueError('Unsupported native VFX sample role weights')
            role='mask' if role_index==3 else 'blend';st_name='_MaskTex' if role=='mask' else '_BlendTex'
            if _number(f.get(name+'MipmapBias'),name+'MipmapBias')!=0:raise ValueError('Unsupported native VFX layer mip bias')
        slot=textures.get(name);st=textures.get(st_name)
        if not isinstance(slot,dict) or not isinstance(st,dict):raise ValueError('Missing native VFX texture '+name)
        texture=slot.get('Texture',{})
        if (not isinstance(texture,dict) or not isinstance(texture.get('Source'),str) or not texture['Source']
            or type(texture.get('PathId')) is not int or not texture['PathId'] or texture.get('Type')!='Texture2D'):
            raise ValueError('Invalid active native VFX texture '+name)
        weights=_vector(c.get(name+'UVWeights'),4,name+'UVWeights')
        if weights!=[1,0,0,0]:raise ValueError('Unsupported native VFX UV weights')
        uv_set='_MainUVSet' if main else name+'UVSet'
        if _number(f.get(uv_set,0),uv_set)!=0:raise ValueError('Unsupported native VFX UV set')
        speed=_vector(c.get(name+'UVSpeed'),4,name+'UVSpeed')
        if speed[2:]!=[0,0]:raise ValueError('Unsupported native VFX custom speed inputs')
        alpha=_number(f.get('_Use'+name[1:]+'AsAlpha'),'sample alpha',0,1)
        if alpha not in (0,1):raise ValueError('Invalid native VFX sample alpha')
        result.append(dict(property=name,role=role,texture=dict(texture),sampleMode='red-as-alpha' if alpha else 'rgba',
            uv=dict(set=0,scaleOffset=_vector(st.get('Scale'),2,st_name+' scale')+_vector(st.get('Offset'),2,st_name+' offset'),
                rotation=_vector(c.get(name+'UVRotateMat'),4,name+'UVRotateMat'),speed=speed[:2])))
    roles={row['role'] for row in result}
    for flag,role in [('_UseMask','mask'),('_UseBlend','blend')]:
        if _number(f.get(flag),flag)!=int(role in roles):raise ValueError('Unsupported native VFX role activation '+flag)
    return result


def layered_vfx_profile(record, texture_indices, *, clock, color_encoding):
    """Convert audited material fields with an explicit host clock binding."""
    if color_encoding not in ('linear','authored-srgb-hdr'):raise ValueError('Invalid native VFX color encoding')
    layers=active_vfx_layers(record)
    if not isinstance(clock,dict) or set(clock)!={'rate','offsetSeconds'}:raise ValueError('Invalid native VFX clock fields')
    clock={key:_number(clock[key],'clock.'+key) for key in ('rate','offsetSeconds')}
    if not isinstance(texture_indices,dict) or set(texture_indices)!={row['property'] for row in layers}:
        raise ValueError('Invalid native VFX texture mapping')
    for value in texture_indices.values():
        if type(value) is not int or not 0<=value<=2147483647:raise ValueError('Invalid native VFX texture index')
    f,c=record['Floats'],record['Colors']
    def color(name):
        value=_vector(c.get(name),4,name)
        if color_encoding=='authored-srgb-hdr':value=[srgb_to_linear_f32(v,hdr=True) for v in value[:3]]+value[3:]
        return value
    tint=color('_TintColor')
    intensity=_number(f.get('_TintColorIntensity'),'_TintColorIntensity');alpha=_number(f.get('_TintColorAlpha'),'_TintColorAlpha')
    tint=[_number(v*intensity,'tint result') for v in tint[:3]]+[_number(tint[3]*alpha,'alpha result')]
    distance=_number(f.get('_SoftDistance'),'_SoftDistance')
    if distance<=0:raise ValueError('Invalid native VFX soft distance')
    surface=dict(schemaVersion=2,lighting='unlit',layers=[dict(role=row['role'],texture=texture_indices[row['property']],
        uv=row['uv'],sampleMode=row['sampleMode']) for row in layers],tint=tint,
        blendTint=color('_BlendTint') if any(row['role']=='blend' for row in layers) else [0,0,0,0],
        clock=clock,softIntersection=dict(distance=distance,bias=_number(f.get('_SoftBias'),'_SoftBias')),
        highlightExpansion=dict(threshold=_number(f.get('_ExpThreshold'),'_ExpThreshold',0),gain=_number(f.get('_ExpIntensity'),'_ExpIntensity',0)),
        postExposureWeight=_number(f.get('_IgnorePostExposure'),'_IgnorePostExposure',0,1),
        globalColorWeight=_number(f.get('_IsSceneEffect'),'_IsSceneEffect',0,1))
    return dict(schemaVersion=2,**{'class':'emissive'},features=['unlit-surface','unlit-texture-layers'],surface=surface)


UNLIT_VFX_SHADER = (
    'CAB-9fe1ba4f3ce8edb2f54eb3e35709a336', -1430105248647086886)


def unlit_vfx_profile(record):
    shader = record.get('Shader', {})
    identity = (shader.get('Source'), shader.get('PathId'))
    if (identity != UNLIT_VFX_SHADER
            or shader.get('Name') != 'HGRP/Effect/VFXBaseV2'):
        raise ValueError('Unverified VFX shader identity')
    switches = record.get('Floats', {})
    for name in ('_UseLighting', '_UseForceDynamicLighting', '_EnvLightStrength'):
        value = switches.get(name)
        if (type(value) not in (int, float) or not math.isfinite(value)
                or value != 0):
            raise ValueError('Unlit VFX requires explicit zero ' + name)
    return {'schemaVersion': 2, 'class': 'emissive',
            'features': ['emissive-mask', 'unlit-surface'],
            'surface': {'schemaVersion': 1, 'lighting': 'unlit'}}
