"""Set mip bias through production ImGui and reopen the image settings."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile

from PIL import Image
from create_playable_project import create
from test_editor_workspace import launch


def run(executable,root):
    root.mkdir(parents=True,exist_ok=False)
    create(root/'game')
    image=root/'game/assets/mip.png'
    Image.new('RGBA',(16,16),(90,120,160,255)).save(image)
    original=hashlib.sha256(image.read_bytes()).hexdigest()
    launch(executable,root,'baseline',(1920,1080),1,capture=False)
    sidecar=Path(str(image)+'.azmeta')
    identity=json.loads(sidecar.read_text(encoding='utf-8'))['id']
    actions=[{'frame':10,'action':'click','target':'assets.search'},
             {'frame':13,'action':'text','text':'mip.png'},
             {'frame':25,'action':'click','target':'asset.'+identity},
             {'frame':29,'action':'click','target':'asset.'+identity},
             {'frame':38,'action':'click','target':'texture.enable.mipBias'},
             {'frame':42,'action':'click','target':'texture.mipBias','ctrl':True},
             {'frame':46,'action':'key','key':'A','down':True,'ctrl':True},
             {'frame':47,'action':'key','key':'A','down':False,'ctrl':True},
             {'frame':48,'action':'key','key':'A','down':False,'ctrl':False},
             {'frame':49,'action':'text','text':'-1'},
             {'frame':52,'action':'key','key':'Enter','down':True},
             {'frame':53,'action':'key','key':'Enter','down':False},
             {'frame':56,'action':'click','target':'texture.enable.mipFilter'},
             {'frame':60,'action':'click','target':'texture.mipFilter'},
             {'frame':64,'action':'click','target':'texture.option.mipFilter.nearest'},
             {'frame':70,'action':'click','target':'texture.plan'},
             {'frame':78,'action':'click','target':'content.apply'},
             {'frame':90,'action':'click','target':'asset.'+identity},
             {'frame':94,'action':'click','target':'asset.'+identity}]
    state=launch(executable,root,'edit-and-reopen',(1920,1080),1,actions=actions)
    metadata=json.loads(sidecar.read_text(encoding='utf-8'))
    assert metadata['id']==identity
    assert metadata['settings']['texture']['mipBias']==-1,metadata
    assert metadata['settings']['texture']['mipFilter']=='nearest',metadata
    assert hashlib.sha256(image.read_bytes()).hexdigest()==original
    assert 'texture.mipBias' in state['widgets'] and 'texture.enable.mipBias' in state['widgets']
    assert not state['uiErrors']
    (root/'summary.json').write_text(json.dumps({'schemaVersion':1,'status':'passed',
        'mipBias':-1,'mipFilter':'nearest','imageIdentity':identity,'sourceImageSha256':original,
        'scope':'Production ImGui scalar entry, texture transaction and reopened metadata'},indent=2),encoding='utf-8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix='sampler-mip-bias-ui-',dir=Path(__file__).resolve().parents[1]/'build/runs') as root:
            run(args.executable.resolve(),Path(root)/'capture')
