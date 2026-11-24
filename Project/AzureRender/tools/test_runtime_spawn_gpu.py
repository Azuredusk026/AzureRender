"""Validate visible runtime spawning with Vulkan and both skinning paths."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
from create_third_person_project import create
from test_independent_animation_gpu import component

def run(executable,root):
    for mode,flags in (('compute',[]),('vertex-fixed',['--disable-compute-skinning','--disable-bindless','--disable-parallel-recording'])):
        project=root/mode;create(project)
        assets=project/'assets';path=assets/'courtyard.azurelevel';level=json.loads(path.read_text(encoding='utf-8'))
        level['nodes'][0]['components']['azure.script']=component('azure.script',{'asset':'assets:/spawn.lua'})
        path.write_text(json.dumps(level),encoding='utf-8')
        (assets/'spawn.lua').write_text("function init() for i=1,24 do self:spawn('spawn-'..i,'cube',{(i%8)-4,1,-math.floor(i/8)-3}) end end",encoding='utf-8')
        report=project/'runtime.json'
        command=[str(executable.resolve()),'--project',str((project/'project.azureproject').resolve()),'--width','640','--height','360',
            '--runtime-report',str(report.resolve()),'--fixed-frame-step','--smoke-frames','24']+flags
        info=subprocess.STARTUPINFO();info.dwFlags|=subprocess.STARTF_USESHOWWINDOW;info.wShowWindow=0
        result=subprocess.run(command,capture_output=True,encoding='utf-8',errors='replace',timeout=120,startupinfo=info)
        (project/'stdout.log').write_text(result.stdout,encoding='utf-8');(project/'stderr.log').write_text(result.stderr,encoding='utf-8')
        assert result.returncode==0,result.stdout+result.stderr
        assert 'VUID-' not in result.stderr and 'Validation Error' not in result.stderr,result.stderr
        assert 'Allocator after unload: buffers=0 images=0' in result.stdout
        data=json.loads(report.read_text(encoding='utf-8'))
        assert not data['scriptErrors'] and not data['presentationErrors'],data
        assert sum(n['id'].startswith('spawn-') and n['visible'] for n in data['nodes'])==24,data['nodes']
    print('Vulkan runtime spawn: 24 visible objects, compute and fixed vertex paths, Validation zero, resources released')

if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--output',type=Path)
    a=p.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure_spawn_') as temporary:run(a.executable,a.output.resolve() if a.output else Path(temporary))
