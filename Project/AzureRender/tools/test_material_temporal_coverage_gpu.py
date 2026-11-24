"""Compare animated material sampling with an independent fourfold reference."""
import argparse
import base64
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import numpy as np
from PIL import Image
import test_projected_hair_integral_gpu as integral
import test_projected_hair_gpu as hair
from temporal_coverage_metrics import correlated_detail


FRAMES=80
FPS=60
DISTANCE=.3


def fixture(folder,filtered,mode,driver="character"):
    project=integral.fixture(folder,True,filtered)
    path=folder/'assets/panels.gltf';asset=json.loads(path.read_text())
    binary=bytearray(base64.b64decode(asset['buffers'][0]['uri'].split(',',1)[1]))
    def append(values,kind,count,component=5126):
        while len(binary)%4:binary.append(0)
        data=struct.pack('<'+('f' if component==5126 else 'H')*len(values),*values)
        view=len(asset['bufferViews']);asset['bufferViews'].append({'buffer':0,'byteOffset':len(binary),'byteLength':len(data)})
        binary.extend(data);accessor=len(asset['accessors'])
        asset['accessors'].append({'bufferView':view,'componentType':component,'count':count,'type':kind})
        return accessor
    for primitive in asset['meshes'][0]['primitives']:
        count=asset['accessors'][primitive['attributes']['POSITION']]['count']
        primitive['attributes']['JOINTS_0']=append([0,0,0,0]*count,'VEC4',count,5123)
        primitive['attributes']['WEIGHTS_0']=append([1,0,0,0]*count,'VEC4',count)
    inverse=append([1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],'MAT4',1)
    joint=len(asset['nodes']);asset['nodes'].append({'name':'MovingJoint'})
    asset['scenes'][0]['nodes'].append(joint);asset['nodes'][0]['skin']=0
    asset['skins']=[{'joints':[joint],'skeleton':joint,'inverseBindMatrices':inverse}]
    times=append([0,16/FPS,48/FPS,2],'SCALAR',4)
    translations=append([0,0,0,0,0,0,DISTANCE,0,0,DISTANCE,0,0],'VEC3',4)
    asset['animations']=[{'name':'translate-and-hold','samplers':[{'input':times,'output':translations,'interpolation':'LINEAR'}],
        'channels':[{'sampler':0,'target':{'node':joint,'path':'translation'}}]}]
    if driver=='camera':asset['animations']=[]
    asset['buffers'][0].update(byteLength=len(binary),uri='data:application/octet-stream;base64,'+base64.b64encode(binary).decode())
    path.write_text(json.dumps(asset))
    path=folder/'assets/scene.azurelevel';level=json.loads(path.read_text())
    level['renderSettings'].update(schemaVersion=19 if mode==4 else 18,antiAliasing=mode)
    if driver=='camera':
        camera=next(node for node in level['nodes'] if node['id']=='camera')
        camera['components']['azure.script']={'type':'azure.script','version':1,
            'data':{'asset':'assets:/camera.lua','enabled':True}}
        (folder/'assets/camera.lua').write_text('function init() self.elapsed=0 end\n'
            'function update(dt)\nself.elapsed=self.elapsed+dt\n'
            'local t=math.max(0,math.min(1,(self.elapsed-16/60)/(32/60)))\n'
            f"self:set('azure.transform','translation',{{-{DISTANCE}*t,0,3}})\nend\n")
    path.write_text(json.dumps(level))
    return project


def frames(folder,scale):
    paths=sorted((folder/'frames').glob('frame_*.png'));assert len(paths)==FRAMES
    values=[]
    for path in paths:
        value=integral.linear(np.asarray(Image.open(path).convert('RGB')))
        assert value.max()<1,'The radiometric sequence must remain unsaturated'
        if scale==4:value=value.reshape(128,4,128,4,3).mean((1,3))
        values.append(value)
    return np.stack(values)


def local(values,frame_lag=0):
    # Align authored translation at its declared render time,
    # independently of measured image flow.
    widthPerWorld=128/(2*hair.CAMERA[2]*math.tan(math.radians(hair.FOV)*.5))
    result=[]
    for index,value in enumerate(values):
        amount=np.clip(((index-frame_lag)/FPS-16/FPS)/(32/FPS),0,1)*DISTANCE*widthPerWorld
        x=np.r_[np.arange(34,57),np.arange(71,94)]+amount
        first=np.floor(x).astype(int);fraction=(x-first)[None,:,None]
        result.append(value[34:94,first]*(1-fraction)+value[34:94,first+1]*fraction)
    return np.stack(result)


def projected_coverage(index,frame_lag=0):
    half=hair.CAMERA[2]*math.tan(math.radians(hair.FOV)*.5)
    coordinates=((np.arange(512)+.5)/512*2-1)*half
    translation=np.clip(((index-frame_lag)/FPS-16/FPS)/(32/FPS),0,1)*DISTANCE
    mask=(np.abs(coordinates[None,:]-translation)<=float(np.float32(.9)))&(np.abs(coordinates[:,None])<=float(np.float32(.9)))
    return mask.reshape(128,4,128,4).any((1,3))


def encoded(values):
    values=np.maximum(values,0)
    return np.rint(np.where(values<=.0031308,values*12.92,1.055*values**(1/2.4)-.055)*255)


def measure(source,candidate,reference,frame_lag=0):
    a,b,ref=(local(v,frame_lag) for v in (source,candidate,reference))
    energy=b.sum((1,2,3))/ref.sum((1,2,3));sourceEnergy=a.sum((1,2,3))/ref.sum((1,2,3))
    residual=b-ref;control=a-ref;moving=slice(16,48)
    error=float(np.abs(residual[moving]).mean());sourceError=float(np.abs(control[moving]).mean())
    change=float(np.abs(np.diff(residual[16:48],axis=0)).mean())
    sourceChange=float(np.abs(np.diff(control[16:48],axis=0)).mean())
    background=reference[:,0:1,0:1]
    centroids=[];edge=[];controlEdge=[];ghosts=[];flatError=0
    for index in range(FRAMES):
        mask=projected_coverage(index,frame_lag)
        assert np.max(np.abs(encoded(reference[index])-encoded(background[index]))[~mask])<=1,('Reference geometry projection mismatch',index)
        centroids.append(float(np.nonzero(mask)[1].mean()))
        padded=np.pad(mask,2);neighbours=[padded[y:y+128,x:x+128] for y in range(5) for x in range(5)]
        outer=np.logical_or.reduce(neighbours);boundary=outer&~np.logical_and.reduce(neighbours)
        flatError=max(flatError,float(np.max(np.abs(encoded(candidate[index])-encoded(background[index]))[~outer])))
        if 16<=index<48:
            edge.append(float(np.abs(candidate[index]-reference[index])[boundary].mean()))
            controlEdge.append(float(np.abs(source[index]-reference[index])[boundary].mean()))
            ghosts.append(int(np.any(np.abs(encoded(candidate[index])-encoded(background[index]))>1,axis=2)[~outer].sum()))
    assert centroids[-1]-centroids[0]>4,'The public motion driver must actually move the fixture'
    return dict(movingEnergyRatios=energy[16:48].tolist(),lastEightEnergyRatios=energy[-8:].tolist(),
        sourceMovingEnergyRatios=sourceEnergy[16:48].tolist(),movingMeanAbsoluteError=error,sourceMovingMeanAbsoluteError=sourceError,
        errorRatio=error/max(sourceError,1e-12),residualChange=change,sourceResidualChange=sourceChange,residualChangeRatio=change/max(sourceChange,1e-12),
        edgeErrorRatio=float(np.mean(edge)/np.mean(controlEdge)),maximumGhostPixelsOutsideTwoPixels=max(ghosts),
        referenceCentroidDisplacement=centroids[-1]-centroids[0],maximumFlatByteError=flatError)


def run(player,output,validation,quality="temporal",driver="character"):
    output.mkdir(parents=True,exist_ok=False);digest=hashlib.sha256(player.read_bytes()).hexdigest()
    report=dict(schemaVersion=1,status='running',qualityMode=quality,motionDriver=driver,playerSha256=digest,frames=FRAMES,fps=FPS,captures=[])
    report['shaders']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (player.parent/'shaders').glob('*.spv')}
    try:
        images={}
        plans=[('source',1,False,0),('integral',1,True,4),('reference',4,False,0)] if quality=='multisample' else [('source',1,False,3),('integral',1,True,3),('reference',4,False,0)]
        for name,scale,filtered,mode in plans:
            folder=output/name;project=fixture(folder,filtered,mode,driver)
            command=[str(player),'--project',str(project),'--width',str(128*scale),'--height',str(128*scale),
                '--capture-dir',str(folder/'frames'),'--capture-frames',str(FRAMES),'--capture-fps',str(FPS),
                '--fixed-frame-step','--qa-animation','--qa-camera','scene','--qa-light','scene','--qa-isolation','hair-kk','--qa-exposure','-4']
            if driver=='camera':command+=['--runtime-report',str(folder/'runtime.json')]
            (folder/'command.json').write_text(json.dumps(command,indent=2))
            with (folder/'player.log').open('w',encoding='utf-8') as log:
                subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
            log=(folder/'player.log').read_text(encoding='utf-8',errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log)
            assert 'Allocator after unload: buffers=0 images=0' in log
            assert ('Validation layer: enabled' in log)==(validation=='enabled')
            manifest=json.loads((folder/'frames/capture_manifest.json').read_text(encoding='utf-8-sig'))
            if mode==4:assert manifest['rasterizationSamples']==4 and not manifest['multisampleFallback'],'Four-sample quality must use actual multisample attachments'
            assert manifest['antiAliasing']==mode and manifest['qaAnimation']
            if mode==3:assert manifest['temporalHistorySubmission']>=FRAMES and manifest['motionOutputActive']
            if driver=='camera':
                state=json.loads((folder/'runtime.json').read_text())
                camera=next(node for node in state['nodes'] if node['id']=='camera')
                assert state['activeScripts']==1 and not state['scriptErrors'] and not state['presentationErrors']
                assert abs(camera['translation'][0]+DISTANCE)<1e-6
            images[name]=frames(folder,scale)
            report['captures'].append(dict(name=name,manifest=manifest,
                images={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((folder/'frames').glob('frame_*.png'))}))
        # Fixed-step world cameras interpolate the previous state at a zero accumulator.
        frame_lag=1 if driver=='camera' else 0
        report['cameraInterpolationFrameLag']=frame_lag
        report['quality']=measure(images['source'],images['integral'],images['reference'],frame_lag);q=report['quality']
        if driver=='camera':
            q['highFrequencyDetailRatios']=correlated_detail(local(images['integral'],frame_lag),
                local(images['reference'],frame_lag),slice(16,48),highpass=True)
            assert all(.8<=value<=1.2 for value in q['highFrequencyDetailRatios']),q
        assert all(abs(v-1)<=.02 for v in q['movingEnergyRatios']+q['lastEightEnergyRatios']),q
        assert q['errorRatio']<=.8 and q['residualChangeRatio']<=.8 and q['edgeErrorRatio']<=.8,q
        assert q['maximumGhostPixelsOutsideTwoPixels']==0,q
        assert q['maximumFlatByteError']<=1,q
        assert hashlib.sha256(player.read_bytes()).hexdigest()==digest;report['status']='passed'
        assert report['shaders']=={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (player.parent/'shaders').glob('*.spv')}
    except Exception as error:report.update(status='failed',error=str(error));raise
    finally:(output/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report['quality']))


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--quality',choices=('temporal','multisample'),default='temporal')
    parser.add_argument('--motion-driver',choices=('character','camera'),default='character')
    parser.add_argument('--output',type=Path);parser.add_argument('--validation',choices=('enabled','disabled'),required=True);args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve(),args.validation,args.quality,args.motion_driver)
    else:
        with tempfile.TemporaryDirectory(prefix='material-motion-') as temporary:
            run(args.executable.resolve(),Path(temporary)/'captures',args.validation,args.quality,args.motion_driver)
