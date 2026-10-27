"""Bind explicitly selected native ECS light profiles to an authored level."""
import copy
import math
import re
import struct

from .captured_lights import decode_directional_header


def _scale(value):
    if type(value) not in (int,float) or not math.isfinite(value) or not 0<=value<=3.4028234663852886e38:
        raise ValueError('Native specular scale requires a finite nonnegative float32 value')
    return struct.unpack('<f',struct.pack('<f',value))[0]


def _profile_color(fields,encoding):
    color=fields.get('color');temperature=fields.get('use_color_temperature')
    if not isinstance(color,list) or len(color) not in (3,4) or type(temperature) is not bool:
        raise ValueError('Profile colors require RGB or RGBA and an explicit temperature flag')
    color=[_scale(value) for value in color]
    if encoding=='authored-srgb' and any(value>1 for value in color):
        raise ValueError('Authored sRGB profile colors require values in [0,1]')
    if temperature:return None
    if encoding=='linear':return color[:3]
    # Round each source operation to float32, including the exponent.
    return [_scale(value/_scale(12.92)) if value<=_scale(.04045) else
        _scale(math.pow(_scale(_scale(value+_scale(.055))/_scale(1.055)),_scale(2.4))) for value in color[:3]]


def bind_specular_profiles(level,profile,*,source_scene,color_encoding=None):
    if color_encoding not in (None,'authored-srgb','linear'):raise ValueError('Unsupported profile color encoding')
    if (not isinstance(profile,dict) or profile.get('format')!='AzureEndfieldEcsLights/1'
            or not isinstance(source_scene,str) or not source_scene or profile.get('scene')!=source_scene
            or not isinstance(profile.get('rows'),list)):
        raise ValueError('Native light profiles require the explicit source scene and ECS format')
    sources={}
    for row in profile['rows']:
        if not isinstance(row,dict) or type(row.get('entity_id')) is not int or not 0<=row['entity_id']<2**63:
            raise ValueError('Native light source identity requires a nonnegative integer')
        identity=row['entity_id']
        if identity in sources:raise ValueError('Duplicate native light source identity')
        if not isinstance(row.get('fields'),dict) or not isinstance(row.get('identity'),str) or not row['identity']:
            raise ValueError('Native light source record lacks its field identity')
        sources[identity]=(_scale(row['fields'].get('specular_intensity')),row['identity'],row['fields'])
    if not isinstance(level,dict) or not isinstance(level.get('lights'),list):
        raise ValueError('Native light binding requires a level light list')
    result=copy.deepcopy(level);bindings=[];unmapped=[];seen=set();entities=set();colored=0;unresolved=[]
    for light in result['lights']:
        if not isinstance(light,dict) or not isinstance(light.get('id'),str) or not light['id']:
            raise ValueError('Invalid target light identity')
        identity=light['id']
        if identity in seen:raise ValueError('Duplicate target light identity')
        seen.add(identity);match=re.fullmatch(r'native-light-(\d+)',identity)
        if not match:
            if identity.startswith('native-light-'):raise ValueError('Malformed native target light identity')
            unmapped.append(identity);continue
        entity=int(match[1])
        if entity in entities:raise ValueError('Duplicate native source binding')
        entities.add(entity)
        if entity not in sources:raise ValueError('Missing native light source: '+identity)
        shape=light.setdefault('shape',{})
        if not isinstance(shape,dict):raise ValueError('Target light shape must be an object')
        value,source_identity,fields=sources[entity];previous=_scale(shape.get('specularScale',1))
        shape['specularScale']=value
        binding=dict(light=identity,sourceEntity=entity,sourceIdentity=source_identity,previous=previous,value=value)
        if color_encoding is not None:
            color=_profile_color(fields,color_encoding)
            binding['color']=dict(source=fields['color'],previous=copy.deepcopy(light.get('color')),
                status='unresolved-temperature' if color is None else 'bound')
            if color is None:unresolved.append(identity)
            else:light['color']=color;binding['color']['value']=color;colored+=1
        bindings.append(binding)
    if not bindings:raise ValueError('Level contains no native lights to bind')
    receipt=dict(schemaVersion=1,sourceScene=source_scene,boundLights=len(bindings),
        changedLights=sum(b['previous']!=b['value'] for b in bindings),unmappedLights=unmapped,bindings=bindings)
    if color_encoding is not None:receipt.update(colorEncoding=color_encoding,coloredLights=colored,unresolvedColorLights=unresolved)
    return result,receipt


def bind_directional_radiometry(level,raw,*,light_id,enabled):
    """Bind captured radiometry; the caller owns the existing local/world direction mapping."""
    decoded=decode_directional_header(raw)
    if type(enabled) is not bool or not isinstance(light_id,str) or not light_id:
        raise ValueError('Main light binding requires an explicit identity and boolean activation')
    if not isinstance(level,dict) or not isinstance(level.get('lights'),list):
        raise ValueError('Main light binding requires a level light list')
    if any(not isinstance(light,dict) for light in level['lights']):raise ValueError('Invalid target light record')
    targets=[light for light in level['lights'] if light.get('id')==light_id]
    if len(targets)!=1 or not isinstance(targets[0].get('shape'),dict) or targets[0]['shape'].get('type')!='directional':
        raise ValueError('Main light binding requires one directional target identity')
    result=copy.deepcopy(level);target=next(light for light in result['lights'] if light.get('id')==light_id)
    previous={key:copy.deepcopy(target.get(key)) for key in ['color','intensity','enabled']}
    previous['specularScale']=target['shape'].get('specularScale',1)
    target.update(color=decoded['color'],intensity=decoded['intensity'],enabled=enabled)
    target['shape']['specularScale']=decoded['specularScale']
    return result,dict(schemaVersion=1,light=light_id,enabled=enabled,previous=previous,
        directionBinding='existing-local-direction',**decoded)
