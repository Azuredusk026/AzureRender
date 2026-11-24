"""Provider routing and credential ownership remain in the optional tool."""
import json
import os
import re
import time
import urllib.request
from urllib.parse import urlparse

def load_config(path):
    if path.stat().st_size>2*1024*1024:raise ValueError('Provider configuration exceeds budget')
    data=json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(data,dict) or set(data)!={'schemaVersion','providers','profiles'} or type(data['schemaVersion']) is not int or data['schemaVersion']!=1:
        raise ValueError('Unsupported provider configuration')
    if not isinstance(data['providers'],list) or len(data['providers'])>64:raise ValueError('Provider count exceeds budget')
    identities=set()
    for row in data['providers']:
        if not isinstance(row,dict) or set(row)!={'id','kind','endpoint','credentialEnv','model','structuredMode'}:raise ValueError('Invalid provider fields')
        if row['kind']!='chat-completions' or row['structuredMode'] not in ('native','json','prompt'):raise ValueError('Unsupported provider capabilities')
        if not isinstance(row['id'],str) or not re.fullmatch(r'[A-Za-z][A-Za-z0-9_.-]{0,127}',row['id']) or row['id'] in identities:raise ValueError('Invalid provider identity')
        identities.add(row['id'])
        address=urlparse(row['endpoint'])
        if address.scheme not in ('http','https') or not address.netloc or address.username or address.password or address.fragment:raise ValueError('Invalid provider endpoint')
        if not isinstance(row['model'],str) or not row['model'] or len(row['model'])>256:raise ValueError('Invalid provider model')
        if not isinstance(row['credentialEnv'],str) or (row['credentialEnv'] and not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]{0,127}',row['credentialEnv'])):raise ValueError('Credential must name a tool-owned environment value')
    if not isinstance(data['profiles'],list) or len(data['profiles'])>64:raise ValueError('Profile count exceeds budget')
    profiles=set()
    for row in data['profiles']:
        if not isinstance(row,dict) or set(row)!={'id','provider','systemPrompt'} or not isinstance(row['id'],str) or not re.fullmatch(r'[A-Za-z][A-Za-z0-9_.-]{0,127}',row['id']):raise ValueError('Invalid profile')
        if row['id'] in profiles or row['provider'] not in identities or not isinstance(row['systemPrompt'],str) or len(row['systemPrompt'].encode('utf-8'))>65536:raise ValueError('Invalid profile reference or prompt')
        profiles.add(row['id'])
    return data

def complete(config,profile,params,check,deadline):
    check()
    provider=next(row for row in config['providers'] if row['id']==profile['provider'])
    credential=os.environ.get(provider['credentialEnv'],'') if provider['credentialEnv'] else ''
    if provider['credentialEnv'] and not credential:raise ValueError('Provider credential unavailable')
    messages=[dict(role='system',content=profile['systemPrompt'])]+params['history']+[dict(role='user',content=params['prompt'])]
    payload=dict(model=provider['model'],messages=messages,stream=False)
    mode=provider['structuredMode']
    if mode=='native':payload['response_format']=dict(type='json_schema',json_schema=dict(name='azure_content',schema=params['schema'],strict=True))
    elif mode=='json':payload['response_format']=dict(type='json_object')
    else:messages[0]['content']+='\nReturn a JSON object matching this schema: '+json.dumps(params['schema'])
    headers={'Content-Type':'application/json'}
    if credential:headers['Authorization']='Bearer '+credential
    request=urllib.request.Request(provider['endpoint'],json.dumps(payload,allow_nan=False).encode('utf-8'),headers=headers,method='POST')
    # The engine process owns cancellation and can terminate this tool at its deadline.
    with urllib.request.urlopen(request,timeout=max(.01,min(10,deadline-time.monotonic()))) as response:
        raw=response.read(2*1024*1024+1)
        if len(raw)>2*1024*1024:raise ValueError('Provider response exceeds budget')
    check()
    result=json.loads(raw,parse_constant=lambda text: (_ for _ in ()).throw(ValueError('Nonfinite provider JSON')))
    content=result['choices'][0]['message']['content']
    if not isinstance(content,str):raise ValueError('Provider result requires text')
    return content,mode
