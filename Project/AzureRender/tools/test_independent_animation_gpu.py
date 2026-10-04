"""Real Player evidence for shared-resource and multi-resource animation."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import numpy as np
from PIL import Image, ImageFilter

def component(name,data,version=1):return {'type':name,'version':version,'data':data}

def run(executable,root,name,asset_root,cpu=False,shift=0,primary=None,outline=True):
    project=root/name;assets=project/'assets';assets.mkdir(parents=True)
    primary=primary or asset_root/'explorer.gltf';primary_name='explorer'+primary.suffix
    shutil.copy2(primary,assets/primary_name);shutil.copy2(asset_root/'guide.gltf',assets/'guide.gltf')
    (project/'project.azureproject').write_text(json.dumps({'schemaVersion':1,'id':name,'name':name,
        'mounts':[{'name':'assets','path':'assets'}],'startupScene':'assets:/scene.azurelevel'}),encoding='utf-8')
    (assets/'motion.json').write_text(json.dumps({'schemaVersion':1,'initial':'idle','states':[{'name':'idle','clip':0},{'name':'walk','clip':1}],'transitions':[]}),encoding='utf-8')
    nodes=[]
    for i,(node,mesh,state,time) in enumerate([('hero-a','explorer','idle',0),('hero-b','explorer','walk',.15+shift),('guide','guide','walk',.4)]):
        nodes.append({'id':node,'resourceId':mesh,'components':{
            'azure.transform':component('azure.transform',{'translation':[(i-1)*1.15,0,0]}),
            'azure.animator':component('azure.animator',{'asset':'assets:/motion.json','state':state,'startTime':time,'morph0':.8 if i==0 else 0},2)}})
    nodes=[nodes[1],nodes[0],nodes[2]]
    (assets/'scene.azurelevel').write_text(json.dumps({'schemaVersion':1,'id':'multi-animation','sceneType':'character',
        'resources':[{'id':'explorer','asset':'assets:/'+primary_name},{'id':'guide','asset':'assets:/guide.gltf'}],'nodes':nodes}),encoding='utf-8')
    capture=root/(name+'-capture');report=root/(name+'-report.json')
    command=[str(executable),'--project',str(project/'project.azureproject'),'--width','960','--height','540',
        '--capture-dir',str(capture),'--capture-frames','4','--capture-fps','4','--runtime-report',str(report),
        '--qa-camera','full-body-front','--qa-light','stylized-key','--qa-animation']
    if cpu:command+=['--disable-compute-skinning','--disable-parallel-recording','--disable-bindless']
    if not outline:command+=['--qa-effect','outline','--qa-effect-state','disabled']
    info=subprocess.STARTUPINFO();info.dwFlags|=subprocess.STARTF_USESHOWWINDOW;info.wShowWindow=0
    result=subprocess.run(command,capture_output=True,encoding='utf-8',errors='replace',timeout=120,startupinfo=info)
    (root/(name+'.stdout.log')).write_text(result.stdout,encoding='utf-8');(root/(name+'.stderr.log')).write_text(result.stderr,encoding='utf-8')
    assert result.returncode==0,result.stdout+result.stderr
    assert 'VUID-' not in result.stderr and 'Validation Error' not in result.stderr,result.stderr
    assert 'Allocator after unload: buffers=0 images=0' in result.stdout,result.stdout
    data=json.loads(report.read_text(encoding='utf-8'));assert not data['presentationErrors'],data
    poses={a['node']:a for a in data['animations']}
    assert set(poses)=={'hero-a','hero-b','guide'},poses
    assert poses['hero-a']['clip']==0 and poses['hero-b']['clip']==poses['guide']['clip']==1,poses
    assert abs((poses['guide']['time']-poses['hero-b']['time'])-(.25-shift))<1e-5,poses
    assert poses['hero-a']['morph'][0]>.7 and poses['hero-b']['morph'][0]==0,poses
    return [np.asarray(Image.open(p).convert('RGB'),dtype=np.int16) for p in sorted(capture.glob('frame_*.png'))]

def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path)
    parser.add_argument('--primary',type=Path)
    args=parser.parse_args();assets=Path(__file__).resolve().parents[1]/'assets_public/third_person'
    with tempfile.TemporaryDirectory(prefix='azure_multi_pose_') as directory:
        root=args.output.resolve() if args.output else Path(directory);root.mkdir(parents=True,exist_ok=True)
        baseline=run(args.executable.resolve(),root,'compute',assets,primary=args.primary)
        cpu=run(args.executable.resolve(),root,'cpu-fixed-serial',assets,True,primary=args.primary)
        shifted=run(args.executable.resolve(),root,'shifted',assets,shift=.25,primary=args.primary)
        no_outline=run(args.executable.resolve(),root,'no-outline',assets,primary=args.primary,outline=False)
        differences=[float(np.abs(a-b).mean()) for a,b in zip(baseline,cpu)]
        assert max(differences)<.1,differences
        changed=int(np.count_nonzero(np.max(np.abs(baseline[1]-shifted[1]),axis=2)>3))
        temporal=int(np.count_nonzero(np.max(np.abs(baseline[0]-baseline[1]),axis=2)>3))
        assert changed>100 and temporal>200,(changed,temporal)
        reference=no_outline[1];background=reference[:,0:1,:]
        geometry=np.max(np.abs(reference-background),axis=2)>8
        expanded=np.asarray(Image.fromarray(geometry.astype(np.uint8)*255).filter(ImageFilter.MaxFilter(13)))>0
        dark_added=(baseline[1][:,:,0]<30)&(baseline[1][:,:,1]<30)&(np.max(np.abs(baseline[1]-reference),axis=2)>5)
        detached=int(np.count_nonzero(dark_added&~expanded))
        assert detached<=10,{'detachedOutlinePixels':detached}
        result={'instances':3,'resources':2,'cpuComputeMeanDifference':differences,
            'independentClockChangedPixels':changed,'animatedPixels':temporal,'detachedOutlinePixels':detached,'validationErrors':0}
        (root/'summary.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result))

if __name__=='__main__':main()
