"""Validate explicit material shadow coverage through production Player frames."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import uuid
import numpy as np
from PIL import Image
from native_render.light_lab import generate
from convert_material_semantics import complete_material_document


def run(player,output,selected=None,expected_validation=None):
    output.mkdir(parents=True,exist_ok=False)
    names=['clear','legacy','geometry','sqrt-alpha','sqrt-alpha-fixed','sqrt-alpha-document','sqrt-alpha-fabric','sqrt-alpha-fade','geometry-fade','multiply-default','sqrt-alpha-multiply']
    if selected:
        assert set(selected)<=set(names);names=[n for n in names if n in selected]
    sha=hashlib.sha256(player.read_bytes()).hexdigest();frames={};summary=dict(schemaVersion=1,status='running',playerSha256=sha,captures=[])
    try:
        for name in names:
            print(name,flush=True);folder=output/name;generate(folder,'directional-near')
            level_path=folder/'level.azurelevel';level=json.loads(level_path.read_text());level['lights'][0]['shape']['direction']=[.6,0,-.8]
            level['renderSettings'].update(shadowRadius=1,shadowDistance=20)
            if name!='clear':
                model=json.loads((folder/'plane.gltf').read_text());material=model['materials'][0];material.update(alphaMode='BLEND',doubleSided=True)
                material.setdefault('extras',{})['azureSlotId']='surface'
                mode='geometry-shadow' if name.startswith('geometry') else 'sqrt-alpha-shadow' if name.startswith('sqrt-alpha') else None
                material['extras']['azureRenderMaterial']=dict(schemaVersion=2,**{'class':'fabric' if name.endswith('fabric') else 'generic'},features=[mode] if mode else [])
                if 'multiply' in name:
                    profile=material['extras']['azureRenderMaterial'];profile['features'].append('multiply-composite');profile['composite']=dict(schemaVersion=1,mode='multiply')
                Image.new('RGBA',(1,1),(255,255,255,64)).save(folder/'alpha.png')
                model.update(images=[dict(uri='alpha.png')],textures=[dict(source=0)])
                material['pbrMetallicRoughness']['baseColorTexture']=dict(index=0)
                if name.endswith('fade'):
                    model['extensionsUsed']=['AZURE_node_opacity']
                    model['nodes'][0]['extensions']={'AZURE_node_opacity':dict(schemaVersion=1,opacity=.5 if name.startswith('sqrt-alpha') else 0)}
                (folder/'caster.gltf').write_text(json.dumps(model));(folder/'caster.gltf.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=str(uuid.uuid4()),settings={},dependencies=[])))
                level['resources'].append(dict(id='caster',asset='assets:/caster.gltf'))
                node=dict(id='caster',resourceId='caster',components={'azure.transform':dict(type='azure.transform',version=1,data=dict(translation=[0,0,1],scale=[.125,.125,1]))})
                if name.endswith('document'):
                    image_id=str(uuid.uuid4());(folder/'alpha.png.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=image_id,settings={},dependencies=[])))
                    document=complete_material_document(material,model,{'0':image_id});material_id=str(uuid.uuid4());(folder/'coverage.azurematerial').write_text(json.dumps(document));(folder/'coverage.azurematerial.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=material_id,settings={},dependencies=[image_id])))
                    node['materialSlots']={'surface':material_id}
                level['nodes'].append(node)
            level_path.write_text(json.dumps(level,indent=2));command=[str(player),'--project',str(folder/'lab.azureproject'),'--width','256','--height','256','--capture-dir',str(folder/'frames'),'--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation','shadow-visibility','--runtime-report',str(folder/'runtime.json')]
            if name.endswith('fixed'):command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            (folder/'command.json').write_text(json.dumps(command,indent=2))
            with (folder/'player.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
            log=(folder/'player.log').read_text(encoding='utf-8',errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log);assert 'Allocator after unload: buffers=0 images=0' in log
            validation='Validation layer: enabled' in log
            if expected_validation:assert validation==(expected_validation=='enabled')
            runtime=json.loads((folder/'runtime.json').read_text());assert not runtime['scriptErrors'] and not runtime['presentationErrors']
            frame=folder/'frames/frame_000000.png';image=np.asarray(Image.open(frame).convert('RGB'));frames[name]=image
            srgb=image/255;linear=np.where(srgb<=.04045,srgb/12.92,((srgb+.055)/1.055)**2.4)
            shadow=float(linear[124:132,148:156].mean());control=float(linear[124:132,88:96].mean())
            if name in ['clear','legacy','geometry-fade','multiply-default']:assert shadow>.98,(name,shadow)
            elif name=='geometry':assert shadow<.05,(name,shadow)
            elif name=='sqrt-alpha-fade':assert .60<shadow<.90,(name,shadow)
            else:assert .35<shadow<.65,(name,shadow)
            assert control>.98,(name,control)
            summary['captures'].append(dict(name=name,shadowVisibility=shadow,controlVisibility=control,validationEnabled=validation,imageSha256=hashlib.sha256(frame.read_bytes()).hexdigest(),allocatorAfterUnload=dict(buffers=0,images=0)))
        if 'sqrt-alpha' in frames:
            for equivalent in ['sqrt-alpha-fixed','sqrt-alpha-document','sqrt-alpha-fabric']:
                if equivalent in frames:assert np.array_equal(frames['sqrt-alpha'],frames[equivalent]),equivalent
        assert hashlib.sha256(player.read_bytes()).hexdigest()==sha
        summary['status']='passed'
    finally:(output/'summary.json').write_text(json.dumps(summary,indent=2))


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',required=True,type=Path);parser.add_argument('--output',type=Path);parser.add_argument('--case',action='append');parser.add_argument('--expected-validation',choices=['enabled','disabled']);args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve(),args.case,args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='azure_shadow_coverage_') as directory:
            output=Path(directory)/'captures';run(args.executable.resolve(),output,args.case,args.expected_validation)
            print((output/'summary.json').read_text())
