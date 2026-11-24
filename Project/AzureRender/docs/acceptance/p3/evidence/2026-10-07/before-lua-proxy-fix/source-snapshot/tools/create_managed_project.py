"""Create public annotation or controller content for optional managed backends."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
from create_third_person_project import create

def make(player,artifacts,backend,workflow,output):
    output=output.resolve();artifacts=artifacts.resolve()
    if output.exists() and any(output.iterdir()):raise ValueError('Managed sample output must be empty')
    if workflow=='controller':create(output)
    else:subprocess.run([str(player.resolve()),'--create-project',str(output)],check=True)
    project=output/'project.azureproject';document=json.loads(project.read_text(encoding='utf-8'))
    managed=output/'assets/managed';managed.mkdir(parents=True,exist_ok=True)
    files=['Azure.Engine.Native.dll'] if backend=='nativeaot' else ['Azure.Engine.dll','Azure.Engine.deps.json','Azure.Engine.runtimeconfig.json','Azure.Samples.dll']
    for name in files:shutil.copy2(artifacts/backend/name,managed/name)
    document['scripting']={'schemaVersion':1,'backend':backend,'module':'assets:/managed/'+files[0]}
    if backend=='coreclr':document['scripting']['runtimeConfig']='assets:/managed/Azure.Engine.runtimeconfig.json'
    descriptor={'schemaVersion':1,'type':'Azure.Samples.SceneAnnotator' if workflow=='annotation' else 'Azure.Samples.ExplorationController'}
    if backend=='coreclr':descriptor['assembly']='assets:/managed/Azure.Samples.dll'
    (output/'assets/behaviour.azscript').write_text(json.dumps(descriptor,indent=2)+'\n')
    script={'type':'azure.script','version':1,'data':{'asset':'assets:/behaviour.azscript','enabled':True}}
    if workflow=='annotation':
        level={'schemaVersion':1,'id':'managed-annotation','sceneType':'sample','renderSettings':{'platform':False},
            'resources':[{'id':'mesh','asset':'engine:/assets_public/test_model.gltf'}],
            'nodes':[{'id':'hero','resourceId':'mesh','components':{'azure.script':script}}]}
        path=output/'assets/annotation.azurelevel';document['startupScene']='assets:/annotation.azurelevel'
    else:
        path=output/'assets/courtyard.azurelevel';level=json.loads(path.read_text(encoding='utf-8'))
        next(n for n in level['nodes'] if n['id']=='hero')['components']['azure.script']=script
    path.write_text(json.dumps(level,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    project.write_text(json.dumps(document,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'project':str(project),'backend':backend,'workflow':workflow,'type':descriptor['type']}))

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--player',type=Path,required=True);parser.add_argument('--artifacts',type=Path,required=True)
    parser.add_argument('--backend',choices=['coreclr','nativeaot'],required=True);parser.add_argument('--workflow',choices=['annotation','controller'],required=True)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    make(args.player,args.artifacts,args.backend,args.workflow,args.output)
