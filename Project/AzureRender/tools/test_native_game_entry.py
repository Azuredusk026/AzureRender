"""Validate installed builder and movable native entry with Unicode paths and isolated PATH."""
import argparse,json,os,subprocess,tempfile
from pathlib import Path

def run(install,output):
    install=install.resolve();output=output.resolve();output.mkdir(parents=True,exist_ok=True)
    root=Path(tempfile.mkdtemp(prefix='Azure Native 中文 '));project=root/'Monica';published=root/'发布 Monica'
    env=os.environ.copy();env['PATH']=str(Path(os.environ['SystemRoot'])/'System32');env.pop('AZURERENDER_RESOURCE_ROOT',None)
    def execute(args):
        result=subprocess.run([str(a) for a in args],cwd=root,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=90)
        with (output/'run.log').open('a',encoding='utf-8') as log:log.write(result.stdout+result.stderr)
        assert result.returncode==0,(result.returncode,result.stdout[-1500:],result.stderr[-1500:])
    execute([install/'bin/AzurePlayer.exe','--create-game',project])
    python=install/'share/AzureRender/tools/python/python.exe'
    execute([python,'-c','import hashlib,json,subprocess,shutil;print("isolated Python ready")'])
    execute([python,install/'share/AzureRender/tools/build_game.py','--project',project/'project.azureproject','--install',install,'--output',published])
    manifest=json.loads((published/'game_manifest.json').read_text(encoding='utf-8'));assert manifest['entry']=='Monica.exe'
    moved=root/'移动后的发布';published.rename(moved)
    execute([moved/manifest['entry'],'--no-error-dialog','--check-project'])
    execute([moved/manifest['entry'],'--no-error-dialog','--smoke-frames','60','--runtime-report',output/'runtime.json'])
    player=moved/'bin/AzurePlayer.exe';hidden=player.with_suffix('.hidden');player.rename(hidden)
    try:
        result=subprocess.run([str(moved/manifest['entry']),'--no-error-dialog'],cwd=root,env=env,timeout=10);assert result.returncode!=0
    finally:hidden.rename(player)
    result={'root':str(root),'install':str(install),'project':str(project/'project.azureproject'),'package':str(moved),'entry':manifest['entry'],'isolatedPython':True,'movedNativeLaunch':True,'missingRuntimeRejected':True}
    (output/'summary.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result))
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--install',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();run(args.install,args.output)
