"""Bind native ECS profiles or explicitly selected captured light records."""
import copy
import math
import re
import struct

from .captured_lights import decode_directional_header,decode_records,render_character_response
from .lighting_ecs import render_light_receivers


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


def bind_captured_lights(level,raw,selection,*,source_scene):
    """Replace explicit light identities with caller-confirmed captured records."""
    fields={'schemaVersion','sourceScene','event','namespace','indices','origin','tileWords','depthWords','surfaceReceivers','replaceLightIds'}
    if (not isinstance(selection,dict) or set(selection)!=fields or type(selection['schemaVersion']) is not int
            or selection['schemaVersion']!=1 or not isinstance(source_scene,str) or not source_scene
            or selection['sourceScene']!=source_scene or type(selection['event']) is not int or selection['event']<=0
            or type(selection['surfaceReceivers']) is not bool or not isinstance(selection['namespace'],str)
            or not re.fullmatch(r'[A-Za-z][A-Za-z0-9_.-]{0,63}',selection['namespace'])):
        raise ValueError('Captured light selection requires explicit version, scene, event, namespace and receivers')
    for name in ['tileWords','depthWords']:
        words=selection[name]
        if not isinstance(words,list) or len(words)!=8 or any(type(w) is not int or not 0<=w<2**32 for w in words):
            raise ValueError('Captured light masks require eight uint32 words')
    indices=selection['indices']
    if (not isinstance(indices,list) or not indices or any(type(i) is not int or not 0<=i<256 for i in indices)
            or len(set(indices))!=len(indices)):
        raise ValueError('Captured light selection requires unique valid indices')
    origin=selection['origin']
    if not isinstance(origin,list) or len(origin)!=3 or any(type(v) not in (int,float) or not math.isfinite(v) for v in origin):
        raise ValueError('Captured origin requires three finite coordinates')
    for index in indices:
        if not (selection['tileWords'][index//32]&selection['depthWords'][index//32]&(1<<(index%32))):
            raise ValueError('Captured light index is outside the confirmed active masks')
    records=decode_records(raw,indices,origin=origin)
    replacements=selection['replaceLightIds']
    if (not isinstance(replacements,list) or any(not isinstance(x,str) or not x for x in replacements)
            or len(set(replacements))!=len(replacements)):
        raise ValueError('Captured light replacement identities must be unique strings')
    if not isinstance(level,dict) or any(not isinstance(level.get(name),list) for name in ['lights','nodes']):
        raise ValueError('Captured light binding requires level nodes and lights')
    identities={}
    for name in ['lights','nodes']:
        values=[row.get('id') if isinstance(row,dict) else None for row in level[name]]
        if any(not isinstance(x,str) or not x for x in values) or len(set(values))!=len(values):
            raise ValueError('Invalid or duplicate captured target '+name)
        identities[name]=set(values)
    if not set(replacements)<=identities['lights']:raise ValueError('Missing captured replacement light identity')
    if len(level['lights'])-len(replacements)+len(records)>128:raise ValueError('Captured snapshot exceeds runtime light capacity')
    result=copy.deepcopy(level);result['lights']=[l for l in result['lights'] if l['id'] not in replacements];bindings=[]
    for record in records:
        if record['mode'] not in (0,16) or record['cookieIndex']>=0 or record['linearLength']!=0:
            raise ValueError('Captured mode, cookie or line emitter requires a separately verified consumer')
        identity=selection['namespace']+'-'+str(record['sourceIndex'])
        if identity in identities['lights'] or identity in identities['nodes']:raise ValueError('Captured light identity collision')
        identities['lights'].add(identity);identities['nodes'].add(identity)
        shape=copy.deepcopy(record['shape']);shape['receivers']=render_light_receivers(dict(npr_type=record['mode'],character_only=not selection['surfaceReceivers']))
        if record['mode']==0:shape['characterResponse']=render_character_response(record)
        node=dict(id=identity,name=identity,parentId='',components={'azure.transform':dict(type='azure.transform',version=1,
            data=dict(translation=record['position'],rotation=[0,0,0],scale=[1,1,1]))})
        result['nodes'].append(node);result['lights'].append(dict(id=identity,nodeId=identity,color=record['color'],intensity=record['intensity'],radius=record['radius'],enabled=True,shape=shape))
        bindings.append(dict(light=identity,sourceIndex=record['sourceIndex'],mode=record['mode'],rawRecordHex=record['rawRecordHex'],rawShadowWords=record['rawShadowWords']))
    receipt=dict(schemaVersion=1,sourceScene=source_scene,event=selection['event'],indices=list(indices),namespace=selection['namespace'],
        replacedLights=len(replacements),replacementIds=list(replacements),createdLights=len(records),origin=list(origin),
        activeMasks={name:list(selection[name]) for name in ['tileWords','depthWords']},
        coordinateConvention='source-world-minus-origin-then-x-reflection',surfaceReceivers=selection['surfaceReceivers'],bindings=bindings)
    return result,receipt
