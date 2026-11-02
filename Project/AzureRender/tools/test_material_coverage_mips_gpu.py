"""Match generated independent cutout mips against standard alpha coverage."""
import argparse,hashlib,json,os,re,subprocess,sys
from pathlib import Path
import numpy as np
from PIL import Image
from test_alpha_mip_coverage_gpu import fixture
from test_material_coverage_gpu import write

def prepare(folder,shape,level,independent):
    extent,side,target=fixture(folder,shape,level)
    path=folder/'plane.gltf';asset=json.loads(path.read_text());material=asset['materials'][0]
    profile=dict(schemaVersion=2,**{'class':'generic'},features=[]);material['extras']=dict(azureRenderMaterial=profile)
    if independent:
        original=np.asarray(Image.open(folder/'coverage.png').convert('RGBA'));mask=original.copy();mask[...,0]=original[...,3];mask[...,3]=255
        Image.fromarray(mask).save(folder/'coverage.png');Image.new('RGBA',(64,64),(255,255,255,0)).save(folder/'base.png')
        asset['images'].append(dict(uri='base.png'));asset['textures'].append(dict(source=1));material['pbrMetallicRoughness']['baseColorTexture']['index']=1
        profile['coverageTexture']=dict(schemaVersion=1,texture=0,channel='red')
    write(path,asset);return folder/'lab.azureproject',extent,side,target

def run(player,output,validation='any',selected=None):
    output.mkdir(parents=True,exist_ok=False);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();report=dict(status='running',programSha256=sha(player),toolSha256=sha(Path(__file__)),cases=[]);images={}
    startup=None
    if hasattr(subprocess,'STARTUPINFO'):startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    env=os.environ.copy();env.setdefault('DOTNET_ROOT','D:/Tools/dotnet9');env.pop('AZURERENDER_RESOURCE_ROOT',None)
    cases=[(shape,level) for shape in ['slant','ring'] for level in [3,4]]
    if selected:
        assert set(selected)<={shape+'-'+str(level) for shape,level in cases};cases=[case for case in cases if case[0]+'-'+str(case[1]) in selected]
    try:
        for shape,level in cases:
            group=shape+'-'+str(level)
            for backend in ['indexed','fixed']:
                for policy in ['reference','independent']:
                    label=group+'-'+backend+'-'+policy;folder=output/label;project,extent,side,target=prepare(folder,shape,level,policy=='independent')
                    command=[str(player),'--project',str(project),'--width',str(extent),'--height',str(extent),'--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation','albedo','--capture-dir',str(folder/'frames')]
                    if backend=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
                    print('START generated coverage',label,flush=True)
                    with (folder/'player.log').open('wb') as log:result=subprocess.run(command,cwd=folder,env=env,startupinfo=startup,stdout=log,stderr=log,timeout=180)
                    assert result.returncode==0,label;text=(folder/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',text),label;assert 'Allocator after unload: buffers=0 images=0' in text,label
                    if validation!='any':assert ('Validation layer: enabled' in text)==(validation=='enabled'),label
                    manifest=json.loads((folder/'frames/capture_manifest.json').read_text());sources=manifest['materialCoverageSources'];assert len(sources)==(policy=='independent')
                    if sources:assert sources[0]['width']==512 and sources[0]['authoredMipLevels']==0
                    frame=folder/'frames/frame_000000.png';image=np.asarray(Image.open(frame).convert('RGB'));images[label]=image;report['cases'].append(dict(name=label,command=command,extent=extent,targetCoverage=target,imageSha256=sha(frame),status='captured'))
                a=images[group+'-'+backend+'-reference'];b=images[group+'-'+backend+'-independent'];error=abs(a.astype(int)-b.astype(int));assert np.array_equal(a,b),(group,backend,int(np.any(error,axis=2).sum()),int(error.max()))
                assert a.max()-a.min()>20,(group,'visible positive control')
            assert np.array_equal(images[group+'-indexed-independent'],images[group+'-fixed-independent']),group
            print('PASS generated coverage',group,flush=True)
        assert sha(player)==report['programSha256'];report['status']='passed'
    finally:
        if report['status']=='running':report['status']='failed'
        write(output/'summary.json',report)

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path);parser.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any');parser.add_argument('--case',action='append');args=parser.parse_args();root=Path(__file__).resolve().parents[1];task=None
    if args.output is None:task=Path(subprocess.check_output([sys.executable,str(root/'tools/artifact_cleanup.py'),'--new-run','material-coverage-mips'],cwd=root,text=True).strip());args.output=task/'captures'
    try:run(args.executable.resolve(),args.output.resolve(),args.expected_validation,args.case)
    finally:
        if task:subprocess.run([sys.executable,str(root/'tools/artifact_cleanup.py'),'--finish',str(task)],cwd=root,check=True)
