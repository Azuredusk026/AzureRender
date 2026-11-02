"""Reject incomplete coverage program families and verify Vulkan cleanup."""
import argparse,hashlib,json,os,re,subprocess,sys
from pathlib import Path
from test_material_coverage_gpu import prepare,write

def run(player,output,expected_validation='any',selected=None):
    output.mkdir(parents=True,exist_ok=False);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    report=dict(status='running',programSha256=sha(player),toolSha256=sha(Path(__file__)),cases=[])
    families={'main':'mesh','snapshot':'character_snapshot_depth','mask':'outline_mask','outline':'outline_view_scaled','outline-depth':'outline_depth','shadow':'shadow'}
    if selected:assert set(selected)<=set(families);families={key:value for key,value in families.items() if key in selected}
    startup=None
    if hasattr(subprocess,'STARTUPINFO'):startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    try:
        for backend in ['indexed','fixed']:
            for family,base in families.items():
                name=backend+'-'+family;folder=output/name;folder.mkdir();suffix=('_bindless' if backend=='indexed' else '')+'_coverage.frag.spv'
                missingNames=[base+suffix]+(['mesh_object'+suffix] if family=='main' else [])
                shaders=folder/'resources/shaders';shaders.mkdir(parents=True);assert all((player.parent/'shaders'/missing).is_file() for missing in missingNames)
                for source in (player.parent/'shaders').glob('*.spv'):
                    if source.name not in missingNames:os.link(source,shaders/source.name)
                project=prepare(folder/'project','hair-shell','independent','red',0)
                command=[str(player),'--project',str(project),'--width','128','--height','128','--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--capture-dir',str(folder/'frames')]
                if backend=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
                env=os.environ.copy();env.setdefault('DOTNET_ROOT','D:/Tools/dotnet9');env['AZURERENDER_RESOURCE_ROOT']=str(shaders.parent)
                print('START missing program',name,flush=True)
                with (folder/'player.log').open('wb') as log:result=subprocess.run(command,cwd=folder,env=env,startupinfo=startup,stdout=log,stderr=log,timeout=180)
                text=(folder/'player.log').read_text(errors='replace');validation='Validation layer: enabled' in text
                selectedMissing=next((missing for missing in missingNames if missing in text),None)
                assert result.returncode!=0 and selectedMissing,name
                if expected_validation!='any':assert validation==(expected_validation=='enabled'),name
                assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[validation error\]|leaked objects',text),name
                assert 'Allocator after unload: buffers=0 images=0' in text,name
                assert not list((folder/'frames').glob('frame_*.png')),name
                report['cases'].append(dict(name=name,status='passed',command=command,exitCode=result.returncode,missingPrograms=missingNames,selectedMissingProgram=selectedMissing,validationEnabled=validation,frames=0,allocatorAfterUnload=dict(buffers=0,images=0)))
        assert sha(player)==report['programSha256'];report['status']='passed'
    finally:
        if report['status']=='running':report['status']='failed'
        write(output/'summary.json',report)

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path);parser.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any');parser.add_argument('--case',action='append');args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];task=None
    if args.output is None:task=Path(subprocess.check_output([sys.executable,str(root/'tools/artifact_cleanup.py'),'--new-run','material-coverage-failure'],cwd=root,text=True).strip());args.output=task/'captures'
    try:run(args.executable.resolve(),args.output.resolve(),args.expected_validation,args.case)
    finally:
        if task:subprocess.run([sys.executable,str(root/'tools/artifact_cleanup.py'),'--finish',str(task)],cwd=root,check=True)
