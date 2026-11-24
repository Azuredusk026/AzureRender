"""Concentrated GPU checks for empty-scene sky and persistent GUI language."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import numpy as np
from PIL import Image
from create_pathtracing_project import create
from test_editor_workspace import launch


def run(executable,root):
    root.mkdir(parents=True)
    images={}
    for name,position,rotation,mode in [('base',[0,1,5],[0,0,0],0),('translated',[10,4,8],[0,0,0],0),('rotated',[0,1,5],[0,90,0],0),('solid',[0,1,5],[0,0,0],2)]:
        folder=root/name;project=create(folder/'project')
        level=project.parent/'assets/room.azurelevel';scene=json.loads(level.read_text(encoding='utf-8'))
        camera=scene['nodes'][-1];camera['components']['azure.transform']['data'].update(translation=position,rotation=rotation)
        scene.update(sceneType='sample',resources=[],nodes=[camera],lights=[])
        scene['renderSettings'].update(renderer='sample',skyMode=mode,skyColor=[.1,.2,.3],outline=0,bloomEnabled=False)
        level.write_text(json.dumps(scene),encoding='utf-8')
        actions=[dict(frame=3,command='edit',operation='viewport.camera-preview',parameters={'node':'camera'})]
        actions += [dict(frame=4,command='edit',operation='viewport.overlay-policy',parameters={'category':c,'policy':'hidden'}) for c in ['grid','camera','light','skeleton']]
        task=folder/'actions.json';task.write_text(json.dumps(actions),encoding='utf-8')
        report=folder/'runtime.json';capture=folder/'capture'
        env=dict(os.environ,AZURERENDER_EDITOR_CONFIG=str(folder/'config'))
        result=subprocess.run([str(executable),'--editor-project',str(project),'--editor-close-policy','discard','--editor-actions',str(task),'--runtime-report',str(report),'--width','1280','--height','720','--capture-dir',str(capture),'--capture-frames','16','--smoke-frames','120','--fixed-frame-step'],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,encoding='utf-8',errors='replace',timeout=90)
        (folder/'run.log').write_text(result.stdout,encoding='utf-8')
        assert result.returncode==0 and '[validation error]' not in result.stdout,result.stdout
        assert 'Allocator after unload: buffers=0 images=0' in result.stdout
        data=json.loads(report.read_text(encoding='utf-8'));assert all(a['passed'] for a in data['editorActions'])
        rect=data['editorWorkspace']['image'];x,y,w,h=[rect[k] for k in ['x','y','width','height']]
        # Lower-middle sky avoids orientation controls and the navigation hint.
        image=Image.open(capture/'frame_000015.png').convert('RGB')
        images[name]=np.asarray(image.crop((int(x+w*.25),int(y+h*.35),int(x+w*.6),int(y+h*.65)))).astype(float)
        for old in capture.glob('frame_*.png'):
            if old.name!='frame_000015.png':old.unlink()
    translation=float(np.abs(images['base']-images['translated']).max())
    rotation=float(np.abs(images['base']-images['rotated']).mean())
    solid=float(images['solid'].std(axis=(0,1)).max())
    assert translation<=1,translation
    assert rotation>1,rotation
    assert solid<1,solid
    language(executable,root/'language')
    summary={'skyTranslationMaxDifference':translation,'skyRotationMeanDifference':rotation,'solidMaxChannelStdDev':solid,'languageSwitchBothDirections':True,'languageRestart':True,'uiErrors':[]}
    (root/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8');print(json.dumps(summary))

def language(executable,ui):
    shutil.copytree(Path(__file__).resolve().parents[1]/'assets_public/scene_inspector',ui/'game',ignore=shutil.ignore_patterns('.azure'))
    def language_actions(value):
        return [dict(frame=20,action='click',target='menu.Edit'),dict(frame=24,action='click',target='menu.preferences'),dict(frame=30,action='click',target='settings.search'),dict(frame=33,action='text',text='editor.language'),dict(frame=38,action='click',target='setting.editor.language'),dict(frame=42,action='click',target='settings.language.'+str(value))]
    english=launch(executable,ui,'english',(1280,720),1,language_actions(1))
    config=ui/'english/config'
    assert json.loads((config/'settings.json').read_text(encoding='utf-8'))['values']['editor.language']==1
    reopened=launch(executable,ui,'english-reopened',(960,640),1,capture=True,config=config)
    assert english['panels']['viewport']['open']==reopened['panels']['viewport']['open']
    chinese=launch(executable,ui,'chinese',(1280,720),1,language_actions(0),config=config)
    assert json.loads((config/'settings.json').read_text(encoding='utf-8'))['values']['editor.language']==0
    print('GUI language switching, narrow layout and persistence passed')

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--language-only',action='store_true')
    args=parser.parse_args();(language if args.language_only else run)(args.executable.resolve(),args.output.resolve())
