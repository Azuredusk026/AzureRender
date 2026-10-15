"""Exercise public demo authoring through the editor production command service."""
import argparse,json,os,subprocess
from pathlib import Path
MODELS=('DamagedHelmet','ABeautifulGame','TransmissionRoughnessTest')
def run(executable,projects,output):
    executable=Path(executable).resolve();output=Path(output).resolve();output.mkdir(parents=True,exist_ok=True)
    results={}
    def launch(name,tag,actions):
        folder=output/name/tag;folder.mkdir(parents=True,exist_ok=True);task=folder/'actions.json';task.write_text(json.dumps(actions),encoding='utf-8')
        env=dict(os.environ,AZURERENDER_EDITOR_CONFIG=str(output/name/'config'))
        proc=subprocess.run([str(executable),'--editor-project',str(Path(projects)/name/'project.azureproject'),'--editor-actions',str(task),'--editor-close-policy','discard','--runtime-report',str(folder/'runtime.json'),'--resource-root',str(folder),'--width','960','--height','640','--smoke-frames','60'],env=env,cwd=folder,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,encoding='utf-8',errors='replace',timeout=300)
        (folder/'run.log').write_text(proc.stdout,encoding='utf-8');assert proc.returncode==0,proc.stdout[-3000:]
        report=json.loads((folder/'runtime.json').read_text(encoding='utf-8'));assert all(a['passed'] for a in report['editorActions']),report['editorActions']
        assert '[validation error]' not in proc.stdout and 'VUID-' not in proc.stdout,proc.stdout[-3000:]
        return report
    for name in MODELS:
        root=Path(projects)/name
        actions=[{'frame':3,'command':'edit','operation':'viewport.camera-preview','parameters':{'node':'camera'}}]
        material=root/'assets/studio.azurematerial'
        if not material.exists():actions.append({'frame':8,'command':'edit','operation':'material.create','parameters':{'type':'azure.pbr','destination':'assets:/studio.azurematerial'}})
        actions.append({'frame':12,'command':'save'});launch(name,'create',actions)
        metadata=json.loads(Path(str(material)+'.azmeta').read_text(encoding='utf-8'));identity=metadata.get('id',metadata.get('uuid'))
        assert identity,metadata
        actions=[{'frame':3,'command':'edit','operation':'viewport.camera-preview','parameters':{'node':'camera'}},
            {'frame':8,'command':'edit','operation':'material.field','parameters':{'asset':identity,'field':'roughness','value':.55}},
            {'frame':10,'command':'edit','operation':'material.assign','parameters':{'nodes':['floor'],'slotId':'name:white','material':identity}},
            {'frame':15,'command':'select','id':'key'}, {'frame':17,'command':'transform','translation':[2.2,3,3]}, {'frame':19,'command':'undo'},
            {'frame':24,'command':'save'}]
        report=launch(name,'edit-save',actions);reopened=launch(name,'reopen',[{'frame':3,'command':'edit','operation':'viewport.camera-preview','parameters':{'node':'camera'}}])
        results[name]={'editedAndSaved':True,'reopened':True,'actions':len(report['editorActions'])}
        print(name,'editor authoring and reopen passed',flush=True)
    (output/'summary.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',required=True);p.add_argument('--projects',required=True);p.add_argument('--output',required=True);a=p.parse_args();run(a.executable,a.projects,a.output)
