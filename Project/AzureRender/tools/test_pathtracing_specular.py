"""A pure metal must respond to a point light with a black environment."""
import argparse,json,subprocess,sys
from pathlib import Path
import numpy as np
from PIL import Image
from create_pathtracing_project import create
def run(exe,root):
    root=Path(root).resolve();root.mkdir(parents=True,exist_ok=True)
    project=root/'project/project.azureproject'
    if not project.exists():project=create(root/'project')
    level=project.parent/'assets/room.azurelevel'
    d=json.loads(level.read_text(encoding='utf-8'))
    d['nodes']=[n for n in d['nodes'] if n['id'] in ('metal-box','key-light','camera')]
    for n in d['nodes']:
        if n['id']=='metal-box':n['components']['azure.transform']['data'].update(translation=[0,1.2,0],scale=[.8,.8,.8])
        if n['id']=='key-light':n['components']['azure.transform']['data']['translation']=[0,1.2,3]
    d['renderSettings'].update(skyColor=[0,0,0],pathTraceBounces=1,pathTraceSamples=8,bloomEnabled=False)
    level.write_text(json.dumps(d),encoding='utf-8')
    script=root/'steps.json';script.write_text(json.dumps({'schemaVersion':1,'steps':[{'op':'wait-frames','frames':12},{'op':'screenshot','label':'specular'},{'op':'wait-frames','frames':3}]}),encoding='utf-8')
    result=subprocess.run([str(Path(exe).resolve()),'--project',str(project),'--width','320','--height','240','--validation-script',str(script),'--validation-report',str(root/'validation.json'),'--resource-root',str(root),'--smoke-frames','24'],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,encoding='utf-8',errors='replace',timeout=90)
    (root/'run.log').write_text(result.stdout,encoding='utf-8');assert result.returncode==0,result.stdout[-2500:]
    image=np.asarray(Image.open(root/'captures/specular.png').convert('RGB')).astype(float)
    value=float(image[60:180,100:220].mean())
    (root/'summary.json').write_text(json.dumps({'metalMean':value,'passed':value>5}),encoding='utf-8')
    assert value>5,f'Pure metal direct lighting missing: mean={value}'
    print('Metal specular response passed:',value)
if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',required=True);p.add_argument('--output',required=True);a=p.parse_args();run(a.executable,a.output)
