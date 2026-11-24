"""Render, publish through editor services, then run moved standalone demos."""
import argparse,json,os,shutil,subprocess,time
from pathlib import Path
from render_pathtracing_demo import run

MODELS=('DamagedHelmet','ABeautifulGame','TransmissionRoughnessTest')

def deliver(install,projects,output,intel,refresh_packages=False):
    install=Path(install).resolve();projects=Path(projects).resolve();output=Path(output).resolve();output.mkdir(parents=True,exist_ok=True)
    results={}
    for name in MODELS:
        project=projects/name/'project.azureproject';case=output/name;case.mkdir(exist_ok=True)
        for denoise in (False,True):
            folder=case/('denoised' if denoise else 'raw')
            if not (folder/'summary.json').exists():run(install/'bin/AzurePlayer.exe',project,folder,(256,),0,(640,480),denoise)
        old={k:os.environ.get(k) for k in ('VK_DRIVER_FILES','VK_ICD_FILENAMES')}
        try:
            for k in old:os.environ[k]=str(Path(intel).resolve())
            if (case/'intel/summary.json').exists():integrated=json.loads((case/'intel/summary.json').read_text(encoding='utf-8'))
            else:integrated=run(install/'bin/AzurePlayer.exe',project,case/'intel',(8,),0,(320,240),True)
        finally:
            for k,v in old.items():
                if v is None:os.environ.pop(k,None)
                else:os.environ[k]=v
        package=case/'package';actions=[
            {'frame':3,'command':'edit','operation':'viewport.camera-preview','parameters':{'node':'camera'}},
            {'frame':10,'command':'play'},{'frame':25,'command':'stop'},{'frame':30,'command':'save'},
            {'frame':35,'command':'build','install':str(install),'output':str(package)},
            {'frame':36,'command':'wait-build'}]
        task=case/'actions.json';task.write_text(json.dumps(actions),encoding='utf-8')
        env=dict(os.environ,AZURERENDER_EDITOR_CONFIG=str(case/'config'))
        start=time.perf_counter()
        destination=projects/'Published'/('中文演示 '+name)
        if refresh_packages or not destination.exists():
            proc=subprocess.run([str(install/'bin/AzureRender.exe'),'--editor-project',str(project),'--editor-actions',str(task),'--editor-close-policy','discard','--runtime-report',str(case/'editor.json'),'--resource-root',str(case),'--width','960','--height','640','--smoke-frames','80'],env=env,cwd=case,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=600)
            (case/'editor.log').write_text(proc.stdout+proc.stderr,encoding='utf-8')
            assert proc.returncode==0,(case,proc.stdout[-3000:],proc.stderr[-1000:])
        editor=json.loads((case/'editor.json').read_text(encoding='utf-8'));assert all(a['passed'] for a in editor['editorActions']),editor['editorActions']
        if refresh_packages or not destination.exists():
            assert package.is_dir(),case
            if destination.exists():
                previous=json.loads((destination/'game_manifest.json').read_text(encoding='utf-8'))
                current=json.loads(project.read_text(encoding='utf-8'))
                assert previous['projectId']==current['id'],destination
                assert destination.parent==projects/'Published' and destination.name=='中文演示 '+name
                shutil.rmtree(destination)
            destination.parent.mkdir(exist_ok=True);shutil.move(str(package),str(destination))
        manifest=json.loads((destination/'game_manifest.json').read_text(encoding='utf-8'))
        assert (destination/'game/assets/licenses/CC-BY-4.0.txt').is_file()
        hidden=project.parent.with_name(name+'.temporarily-hidden');assert not hidden.exists()
        project.parent.rename(hidden)
        try:
            proc=subprocess.run([str(destination/manifest['entry']),'--no-error-dialog','--smoke-frames','24','--width','320','--height','240','--runtime-report',str(case/'standalone.json')],cwd=case,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=300)
        finally:hidden.rename(project.parent)
        log=(destination/'captures/game-startup.log').read_text(encoding='utf-8',errors='replace')
        (case/'standalone.log').write_text(log+proc.stdout+proc.stderr,encoding='utf-8')
        assert proc.returncode==0,(case,proc.stdout[-3000:],proc.stderr[-1000:])
        runtime=json.loads((case/'standalone.json').read_text(encoding='utf-8'));assert runtime['renderer']=='pathtracing',runtime
        assert 'Allocator after unload: buffers=0 images=0' in log,log[-2000:]
        assert '[validation error]' not in log and 'VUID-' not in log
        results[name]={'editorBuild':True,'playStop':True,'movedPackage':str(destination),'entry':manifest['entry'],'sourceUnavailable':True,'standalone':True,'intel':integrated,'publishAndLaunchSeconds':time.perf_counter()-start}
        (output/'summary.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
        print(name,'delivery passed',flush=True)
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__)
    for arg in ('install','projects','output','intel'):p.add_argument('--'+arg,required=True)
    p.add_argument('--refresh-packages',action='store_true')
    a=p.parse_args();deliver(a.install,a.projects,a.output,a.intel,a.refresh_packages)
