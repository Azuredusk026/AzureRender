"""Verify placed geometry changes actual editor pixels in a scene template."""
import argparse,json,shutil,uuid
from pathlib import Path
from PIL import Image,ImageChops
from editor_test_host import EditorHost

def run(editor,output):
    source=Path(__file__).resolve().parents[1]/'assets_public'
    out=output/('run-'+uuid.uuid4().hex[:8]);out.mkdir(parents=True)
    seed=out/'seed';shutil.copytree(source/'scene_inspector',seed,ignore=shutil.ignore_patterns('.azure'))
    project=out/'authored'
    with EditorHost(editor,seed/'project.azureproject',out/'create') as host:
        host.edit('project.create',templateId='scene',destination=str(project),name='Geometry workflow')
        host.frames(20)
    with EditorHost(editor,project/'project.azureproject',out/'geometry') as host:
        asset=host.edit('asset.import',path=str(source/'scene_inspector/assets/cube.gltf'))
        host.edit('node.place',resource=asset,id='authored-cube')
        host.edit('node.select',id='authored-cube');host.edit('viewport.frame-selection')
        host.frames(20)
        shown=host.screenshot('shown');rect=host.workspace()['image']
        host.edit('node.visible',value=False);host.frames(20)
        hidden=host.screenshot('hidden')
        box=(int(rect['x'])+5,int(rect['y'])+5,int(rect['x']+rect['width'])-5,int(rect['y']+rect['height'])-5)
        diff=ImageChops.difference(Image.open(shown).convert('RGB').crop(box),Image.open(hidden).convert('RGB').crop(box))
        changed=sum(max(pixel)>12 for pixel in diff.getdata())
        assert changed>1000,('Placed geometry must render, not only exist in the document',changed)
        host.edit('node.visible',value=True);host.edit('document.save')
    (out/'summary.json').write_text(json.dumps(dict(passed=True,changedPixels=changed,physicalInput=False,
        publicPlacement=True,visibility=True,project=str(project)),indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--editor',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();run(a.editor.resolve(),a.output.resolve())
