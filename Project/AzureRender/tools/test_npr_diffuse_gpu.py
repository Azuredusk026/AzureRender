"""Exercise explicit NPR composition through real scene and volume lighting.

The CPU oracle follows the literal source expression independently from the
GLSL helper. Native runtime globals remain outside this host fixture.
"""
import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import numpy as np
from PIL import Image
from native_render.light_lab import generate
from create_visibility_fixture import surface,component


def linear(value):
    value=np.asarray(value,dtype=float)/255
    return np.where(value<=.04045,value/12.92,((value+.055)/1.055)**2.4)


def reference(base,shadow,ramp,ao,scale,character_visibility=1,shadow_mask=0):
    # Literal source dependencies with independent material receiver selection.
    lit=base*.95999997854232788
    source=shadow*.95999997854232788*scale
    weights=np.array([.21267290413379669,.7151522040367126,.07217500358819962])
    saturated=lambda c:np.dot(c,weights)+(c-np.dot(c,weights))*1.2000000476837158
    shade=saturated(source*.64999997615814209)
    visibility=1-shadow_mask+character_visibility*shadow_mask
    shade=shade+(source-shade)*np.clip(ao*((1-shadow_mask)+visibility*shadow_mask)+ramp[3],0,1)
    untinted=shade+(lit-shade)*min(ao,visibility,ramp[3])
    chroma=max(ramp[:3])-min(ramp[:3])
    tinted=untinted*(1-chroma+ramp[:3]*chroma)
    return tinted*np.clip(np.dot(untinted,weights)/max(np.dot(tinted,weights),.0010000000474974513),0,1.5)


def run(player,output,require_validation=False):
    output.mkdir(parents=True,exist_ok=False)
    cases=[('volume-shadow',True,0,0,.7,False,False),
        ('volume-lit',True,255,255,.7,False,False),
        ('volume-ramp-zero',True,0,255,.7,False,False),
        ('volume-scale',True,0,0,.35,False,False),
        ('volume-fixed',True,0,0,.7,True,False),
        ('indirect-default',True,0,0,.7,False,False),
        ('indirect-surface',True,0,0,.7,False,False),
        ('indirect-horizontal',True,0,0,.7,False,False),
        ('indirect-horizontal-fixed',True,0,0,.7,True,False),
        ('face-radial',True,0,0,.7,False,False),
        ('face-radial-mask',True,0,0,.7,False,False),
        ('face-radial-shift',True,0,0,.7,False,False),
        ('face-radial-fixed',True,0,0,.7,True,False),
        ('face-surface',True,0,0,.7,False,False),
        ('face-radial-sdf-off',True,0,0,.7,False,False),
        ('face-surface-sdf-off',True,0,0,.7,False,False),
        ('face-surface-flat-sdf-off',True,0,0,.7,False,False),
        ('direct-shadow',False,0,0,.7,False,False),
        ('direct-lit',False,255,255,.7,False,False),
        ('direct-fixed',False,0,0,.7,True,False),
        ('local-front',False,0,0,.7,False,False),('local-back',False,0,0,.7,False,False),
        ('local-side',False,0,0,.7,False,False),
        ('surface-receiver',False,0,0,.7,False,False),('character-receiver',False,0,0,.7,False,False),
        ('none-receiver',False,0,0,.7,False,False),('character-only-plain',False,0,0,.7,False,False),
        ('main-shadow-field',False,0,0,.7,False,False),('main-shadow',False,0,0,.7,False,False),
        ('main-shadow-disabled',False,0,0,.7,False,False),
        ('dark',False,0,0,.7,False,True),
        ('plain-pbr',True,0,0,.7,False,False),
        ('specular-npr',False,0,0,.7,False,False),('specular-plain',False,0,0,.7,False,False),
        ('emissive-npr',False,0,0,.7,False,False),('emissive-plain',False,0,0,.7,False,False)]
    results=[];frames={};controls={};face_controls={}
    for name,volume,ramp_alpha,ao,scale,fixed,dark in cases:
        local=name.startswith('local-')
        face=name.startswith('face-')
        indirect=name.startswith('indirect-') or face
        folder=output/name;generate(folder,'point' if local else 'directional-near' if not volume and not dark else 'dark')
        model=json.loads((folder/'plane.gltf').read_text())
        Image.new('RGBA',(1,1),(200,140,90,ao)).save(folder/'base.png')
        Image.new('RGBA',(4,2),(120,50,20,255)).save(folder/'lut.png')
        Image.new('RGBA',(2,1),(180,150,170,ramp_alpha)).save(folder/'ramp.png')
        model.update(images=[{'uri':p+'.png'} for p in ['base','lut','ramp']],textures=[{'source':i} for i in range(3)])
        material=model['materials'][0];material['pbrMetallicRoughness'].update(baseColorTexture={'index':0},baseColorFactor=[1,1,1,1])
        profile={'schemaVersion':2,'class':'generic','features':['diffuse-ramp'],
            'shadowColorLut':{'schemaVersion':1,'texture':1,'flipV':False},
            'diffuseRamp':{'schemaVersion':1,'texture':2},
            'styleParameters':[1,1,0,0],
            'nprDiffuse':{'schemaVersion':1,'shadowScale':scale,'lightingInputs':'host-scene-radiance',
                'shadowMask':'none','baseAo':'albedo-alpha'}}
        if indirect:
            if face:
                profile.update({'class':'face','features':['face-sdf-eligible']})
                profile.pop('diffuseRamp')
                profile['nprDiffuse']['indirectNormal']='surface' if name.startswith('face-surface') else 'face-frame-radial-horizontal'
                Image.new('RGBA',(1,1),(128,128,230,255)).save(folder/'sdf.png')
                Image.new('RGBA',(1,1),(0,128 if name=='face-radial-mask' else 0,0,255)).save(folder/'mask.png')
                model['images'] += [{'uri':'sdf.png'},{'uri':'mask.png'}]
                model['textures'] += [{'source':3},{'source':4}]
                if name.endswith('-sdf-off') and name!='face-surface-flat-sdf-off':
                    Image.new('RGBA',(1,1),(64,217,191,255)).save(folder/'normal.png')
                    model['images'].append({'uri':'normal.png'})
                    model['textures'].append({'source':5})
                    material['normalTexture']={'index':5,'scale':1}
                model['nodes'].append({'name':'FaceHead','translation':[1 if name=='face-radial-shift' else 0,0,0]})
                model['scenes'][0]['nodes'].append(1)
                profile['faceSdf']={'schemaVersion':2,'texture':3,'maskTexture':4,'rampTexture':2,
                    'texCoord':0,'headNode':'FaceHead','headBasis':[1,0,0,0,1,0,0,0,1]}
            elif name!='indirect-default':
                profile['nprDiffuse']['indirectNormal']='world-horizontal' if 'horizontal' in name else 'surface'
            # Attribute and view offsets identify the authored tilted world normal.
            accessor=model['accessors'][model['meshes'][0]['primitives'][0]['attributes']['NORMAL']]
            view=model['bufferViews'][accessor['bufferView']]
            buffer=model['buffers'][view['buffer']]
            blob=bytearray(base64.b64decode(buffer['uri'].split(',',1)[1]))
            for vertex in range(accessor['count']):
                offset=view.get('byteOffset',0)+accessor.get('byteOffset',0)+vertex*view.get('byteStride',12)
                struct.pack_into('<3f',blob,offset,0,.8,.6)
            buffer['uri']='data:application/octet-stream;base64,'+base64.b64encode(blob).decode()
        control=name.startswith(('specular-','emissive-'))
        if name=='plain-pbr' or name.endswith('-plain'):profile.pop('nprDiffuse')
        if name.startswith('emissive-'):
            profile['features'].append('emissive-mask');material['emissiveFactor']=[.05,.1,.2]
        material['extras']={'azureRenderMaterial':profile}
        (folder/'plane.gltf').write_text(json.dumps(model))
        level=json.loads((folder/'level.azurelevel').read_text())
        level['renderSettings'].update(exposure=0,saturation=1,contrast=1)
        if face:level['renderSettings']['faceSdf']=True
        if name in ['local-back','local-side']:
            next(n for n in level['nodes'] if n['id']=='light')['components']['azure.transform']['data']['translation']=[0,0,-2] if name=='local-back' else [2,0,0]
        if 'receiver' in name or name=='character-only-plain':
            level['lights'][0]['shape']['receivers']={'schemaVersion':1,'surfaces':name=='surface-receiver',
                'characters':name in ['character-receiver','character-only-plain']}
        if name.startswith('main-shadow'):
            surface(folder/'blocker.gltf')
            level['resources'].append({'id':'blocker','asset':'assets:/blocker.gltf'})
            level['nodes'].append({'id':'blocker','resourceId':'blocker','components':{
                'azure.transform':component('azure.transform',{'translation':[1,0,1],'scale':[.6,.6,.6]})}})
            level['lights'][0]['shape']['direction']=[-2**-.5,0,-2**-.5]
        if volume:
            cell=[0,0,0,.7,0,0,0,.5,0,0,0,.3,1,0,0,0]
            if indirect:cell=[0,.3,.1,.2,0,-.1,.2,.3,0,.05,.15,.1,1,0,0,0]
            if face:cell=[.3,0,-.3,.5,-.1,-.1,.2,.3,.02,.05,.15,.1,1,0,0,0]
            (folder/'field.aziv').write_bytes(struct.pack('<8s4f3I',b'AZIV0001',-4,-4,-4,8,2,2,2)+struct.pack('<128f',*(cell*8)))
            level['irradianceVolume']={'version':1,'asset':'assets:/field.aziv','enabled':True,'intensity':1}
        (folder/'level.azurelevel').write_text(json.dumps(level))
        command=[str(player.resolve()),'--project',str(folder/'lab.azureproject'),
            '--width','128','--height','128','--capture-dir',str(folder/'frames'),
            '--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene',
            '--runtime-report',str(folder/'runtime.json')]
        if name.startswith('main-shadow'):
            command+=['--qa-isolation','shadow-visibility' if name.endswith('-field') else 'direct-diffuse']
            if name.endswith('-disabled'):command+=['--qa-effect','shadow','--qa-effect-state','disabled']
        else:command+=['--qa-isolation','direct-specular' if name.startswith('specular-') else 'emissive'] if control else ['--qa-effect','specular','--qa-effect-state','disabled']
        if face:
            command+=['--qa-isolation','ambient']
            if name.endswith('-sdf-off'):
                # This override replaces the specular diagnostic switch; the
                # ambient-only isolation excludes specular independently.
                at=command.index('--qa-effect');command[at+1]='face-sdf'
        if fixed:command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
        (folder/'command.json').write_text(json.dumps(command,indent=2))
        with (folder/'player.log').open('w',encoding='utf8') as stream:
            subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT,check=True,timeout=120)
        log=(folder/'player.log').read_text(encoding='utf8',errors='replace')
        assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
        assert 'Allocator after unload: buffers=0 images=0' in log,name
        if require_validation:assert 'Validation layer: enabled' in log,name
        runtime=json.loads((folder/'runtime.json').read_text())
        assert not runtime['scriptErrors'] and not runtime['presentationErrors'],name
        image=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'))
        frames[name]=image
        if name.endswith('-sdf-off'):
            face_controls[name]=dict(center=image[64,64].tolist(),validationEnabled='Validation layer: enabled' in log)
            continue
        if control:
            controls[name]=dict(center=image[64,64].tolist(),validationEnabled='Validation layer: enabled' in log)
            continue
        expected=linear([200,140,90]) if name=='plain-pbr' else reference(linear([200,140,90]),linear([120,50,20]),np.array([180,150,170,ramp_alpha])/255,ao/255,scale)
        if indirect:
            # Source arithmetic is evaluated independently in double precision.
            normal=np.array([0.,.8,.6])
            if face and not name.startswith('face-surface'):
                position=np.array([.03125,-.03125,0.])
                origin=np.array([1. if name=='face-radial-shift' else 0.,0.,0.])
                radial=np.array([position[0]-origin[0],2.**-14,position[2]-origin[2]])
                radial/=np.linalg.norm(radial)
                geometry=np.array([normal[0],2.**-14,normal[2]])
                geometry/=np.linalg.norm(geometry)
                mask=128/255 if name=='face-radial-mask' else 0.
                normal=radial*(1-mask)+geometry*mask
                normal=normal/np.linalg.norm(normal) if np.dot(normal,normal)>1e-12 else geometry
            elif 'horizontal' in name:
                normal=np.array([normal[0],2.**-14,normal[2]])
                normal/=np.linalg.norm(normal)
            coefficients=np.array([[0,.3,.1,.2],[0,-.1,.2,.3],[0,.05,.15,.1]])
            if face:coefficients=np.array([[.3,0,-.3,.5],[-.1,-.1,.2,.3],[.02,.05,.15,.1]])
            expected*=np.maximum(coefficients@np.append(normal,1.),0)
        else:expected*=np.array([.7,.5,.3]) if volume else 0 if dark else .3
        if name in ['surface-receiver','none-receiver','character-only-plain','main-shadow','main-shadow-field']:expected*=0
        if local:
            position=np.array([.03125,-.03125,0])
            light=np.array([2,0,0] if name=='local-side' else [0,0,-2 if name=='local-back' else 2])
            direction=light-position;distance=np.linalg.norm(direction)
            weight=(1-distance/10)**2/(1+.12*distance*distance)
            expected*=3*weight*max(direction[2]/distance,0)/.3
        actual=linear(image[64,64]);error=float(np.max(np.abs(actual-expected)))
        results.append(dict(name=name,actual=actual.tolist(),expected=expected.tolist(),maximumLinearError=error,
            validationEnabled='Validation layer: enabled' in log))
    summary=dict(status='captured',playerSha256=hashlib.sha256(player.read_bytes()).hexdigest(),
        scope='explicit-host-lighting',budget=.008,cases=results,independentLayerControls=controls,faceIndirectControls=face_controls)
    (output/'summary.json').write_text(json.dumps(summary,indent=2))
    assert max(c['maximumLinearError'] for c in results)<.008,results
    for a,b in [('volume-shadow','volume-fixed'),('direct-shadow','direct-fixed')]:assert np.array_equal(frames[a],frames[b]),(a,b)
    assert np.array_equal(frames['indirect-default'],frames['indirect-surface']),'Explicit surface mode must preserve the default'
    assert np.array_equal(frames['indirect-horizontal'],frames['indirect-horizontal-fixed']),'Horizontal indirect mode must match fixed and bindless descriptors'
    assert np.max(np.abs(linear(frames['indirect-default'][64,64])-linear(frames['indirect-horizontal'][64,64])))>.005,'Directional SH must exercise the horizontal normal selector'
    assert np.array_equal(frames['face-radial'],frames['face-radial-fixed']),'Face radial mode must match fixed and bindless descriptors'
    assert np.array_equal(frames['face-radial-sdf-off'],frames['face-surface-sdf-off']),'Disabled SDF must fall back to the surface normal'
    assert max(face_controls['face-radial-sdf-off']['center'])>0,'Disabled-SDF fallback requires a visible positive control'
    assert np.max(np.abs(linear(frames['face-surface-sdf-off'][64,64])-linear(frames['face-surface-flat-sdf-off'][64,64])))>.005,'Disabled-SDF controls must consume the non-neutral normal map'
    for name in ['face-radial-mask','face-radial-shift','face-surface']:
        assert np.max(np.abs(linear(frames['face-radial'][64,64])-linear(frames[name][64,64])))>.005,('Face radial input must change directional SH',name)
    for layer in ['specular','emissive']:
        assert np.array_equal(frames[layer+'-npr'],frames[layer+'-plain']),layer+' must remain independent from diffuse composition'
        assert max(controls[layer+'-npr']['center'])>0,layer+' positive control missing'
    summary['status']='passed'
    (output/'summary.json').write_text(json.dumps(summary,indent=2))
    summary['captures']=len(cases)
    summary['maximumLinearError']=max(c['maximumLinearError'] for c in results)
    (output/'summary.json').write_text(json.dumps(summary,indent=2))
    print(json.dumps(summary))


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path);parser.add_argument('--require-validation',action='store_true')
    args=parser.parse_args()
    if args.output:run(args.executable,args.output.resolve(),args.require_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='npr-diffuse-',dir=Path(__file__).resolve().parents[1]/'build/runs') as folder:
            run(args.executable,Path(folder)/'captures',args.require_validation)
