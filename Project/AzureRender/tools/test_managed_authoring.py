"""Compile registered managed templates and execute production callbacks."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import uuid
from build_managed_scripts import project

def run(tester,artifacts,output):
    root=output/('run-'+uuid.uuid4().hex[:8]);root.mkdir(parents=True)
    game=root/'game';subprocess.run([str(tester),'create',str(game)],check=True)
    sources=sorted((game/'assets').glob('*.cs'))
    for source in sources:
        source.write_text(source.read_text().replace('public override void Init() { }','public override void Init() { Self.Set("azure.transform", "translation", new System.Text.Json.Nodes.JsonArray(13, 2, 3)); }'),encoding='utf-8')
    compiled=game/'assets/compiled';compiled.mkdir()
    for name in ('Azure.Engine.dll','Azure.Engine.runtimeconfig.json','Azure.Engine.deps.json'):shutil.copy2(artifacts/name,compiled/name)
    project(root/'ProjectScripts.csproj','ProjectScripts',sources,reference=compiled/'Azure.Engine.dll')
    subprocess.run(['dotnet','build',str(root/'ProjectScripts.csproj'),'-c','Release','-o',str(compiled),'--nologo'],check=True)
    manifest=game/'project.azureproject';document=json.loads(manifest.read_text(encoding='utf-8'))
    document['scripting']=dict(schemaVersion=1,backend='coreclr',module='assets:/compiled/Azure.Engine.dll',runtimeConfig='assets:/compiled/Azure.Engine.runtimeconfig.json')
    document['startupScene']='assets:/templates.azurelevel';manifest.write_text(json.dumps(document,indent=2)+'\n',encoding='utf-8')
    nodes=[]
    for name,kind in [('CoreController','core'),('NativeController','native')]:
        (game/f'assets/{name}.azscript').write_text(json.dumps(dict(schemaVersion=1,type='ProjectScripts.'+name,assembly='assets:/compiled/ProjectScripts.dll'))+'\n')
        nodes.append(dict(id=kind,components={'azure.script':dict(type='azure.script',version=1,data=dict(asset=f'assets:/{name}.azscript',enabled=True))}))
    (game/'assets/templates.azurelevel').write_text(json.dumps(dict(schemaVersion=1,id='generated-templates',sceneType='sample',resources=[],nodes=nodes),indent=2)+'\n')
    subprocess.run([str(tester),'verify',str(game)],check=True)
    (root/'summary.json').write_text(json.dumps(dict(templates=2,compiler='net9.0',runtime='coreclr',callbacks=True),indent=2)+'\n')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--tester',type=Path,required=True);parser.add_argument('--artifacts',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();run(args.tester.resolve(),args.artifacts.resolve(),args.output.resolve())
