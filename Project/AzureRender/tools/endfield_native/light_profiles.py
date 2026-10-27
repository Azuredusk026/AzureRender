"""Bind explicitly selected native ECS light profiles to an authored level."""
import copy
import math
import re
import struct


def _scale(value):
    if type(value) not in (int,float) or not math.isfinite(value) or not 0<=value<=3.4028234663852886e38:
        raise ValueError('Native specular scale requires a finite nonnegative float32 value')
    return struct.unpack('<f',struct.pack('<f',value))[0]


def bind_specular_profiles(level,profile,*,source_scene):
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
        sources[identity]=(_scale(row['fields'].get('specular_intensity')),row['identity'])
    if not isinstance(level,dict) or not isinstance(level.get('lights'),list):
        raise ValueError('Native light binding requires a level light list')
    result=copy.deepcopy(level);bindings=[];unmapped=[];seen=set();entities=set()
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
        value,source_identity=sources[entity];previous=_scale(shape.get('specularScale',1))
        shape['specularScale']=value
        bindings.append(dict(light=identity,sourceEntity=entity,sourceIdentity=source_identity,previous=previous,value=value))
    if not bindings:raise ValueError('Level contains no native lights to bind')
    return result,dict(schemaVersion=1,sourceScene=source_scene,boundLights=len(bindings),
        changedLights=sum(b['previous']!=b['value'] for b in bindings),unmappedLights=unmapped,bindings=bindings)
