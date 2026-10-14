"""Owned editor process and authenticated production validation client."""
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import time
from run_playable_long_run import NativeWindow
from test_validation_protocol import exchange

class EditorHost:
    def __init__(self, executable, project, output):
        self.executable=Path(executable)
        self.output=Path(output);self.output.mkdir(parents=True,exist_ok=True)
        self.token=secrets.token_hex(32);self.evidence=[]
        self.endpoint_file=self.output/'endpoint.json'
        self.logs=[(self.output/name).open('w',encoding='utf-8') for name in ('stdout.log','stderr.log')]
        env=dict(os.environ,AZURE_AUTHORING_TEST_TOKEN=self.token,AZURERENDER_EDITOR_CONFIG=str(self.output/'config'),AZURERENDER_EDITOR_DPI='1')
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        command=[str(executable),'--editor-project',str(project),'--editor-close-policy','discard','--width','1920','--height','1080',
            '--fixed-frame-step','--validation-token-env','AZURE_AUTHORING_TEST_TOKEN','--validation-endpoint',str(self.endpoint_file),'--runtime-report',str(self.output/'runtime.json')]
        self.process=subprocess.Popen(command,cwd=self.output,env=env,startupinfo=startup,stdout=self.logs[0],stderr=self.logs[1])
    def __enter__(self):
        try:
            deadline=time.monotonic()+45
            while not self.endpoint_file.exists():
                assert self.process.poll() is None,'Editor exited during startup'
                assert time.monotonic()<deadline,'Editor startup timeout'
                time.sleep(.02)
            self.endpoint=json.loads(self.endpoint_file.read_text(encoding='utf-8'));self.frames();return self
        except BaseException:
            self.__exit__(None,None,None);raise
    def __exit__(self,*_):
        try:
            if self.process.poll() is None:
                window=NativeWindow(self.process.pid)
                if window.find():window.close()
                else:self.process.terminate()
                try:self.process.wait(timeout=60)
                except subprocess.TimeoutExpired:self.process.kill();self.process.wait(timeout=10)
        finally:
            for log in self.logs:log.close()
            (self.output/'evidence.json').write_text(json.dumps(self.evidence,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        if not any(_):
            assert self.process.returncode==0,self.process.returncode
            stdout=(self.output/'stdout.log').read_text(encoding='utf-8');stderr=(self.output/'stderr.log').read_text(encoding='utf-8')
            assert 'Allocator after unload: buffers=0 images=0' in stdout
            assert 'VUID-' not in stderr and 'Validation Error' not in stderr
    def call(self,op,passed=True,**fields):
        result=exchange(self.endpoint,self.token,dict(op=op,**fields));self.evidence.append(dict(op=op,fields=fields,response=result))
        assert result['passed']==passed,result
        return result.get('value')
    def query(self,name):return self.call('query',name=name)
    def workspace(self):return json.loads(self.query('editor.workspace'))
    def edit(self,command,passed=True,**parameters):
        result=self.call('edit',passed=passed,requestId='authoring-'+str(len(self.evidence)),commandId=command,parameters=parameters,baseVersion=json.loads(self.query('document.version')))
        return result.get('value') if passed else result
    def frames(self,count=6):self.call('wait-frames',frames=count)
    def click(self,target,**fields):
        if target.startswith('tool.') and not self.workspace()['panels'][target[5:]]['open']:
            self.click('menu.View')
            target='panel.'+target[5:]
        self.call('input',event=dict(action='click',target=target,**fields));self.frames()
    def screenshot(self,label):
        self.call('screenshot',label=label);self.frames()
        source=self.executable.parent/'captures'/f'{label}.png'
        assert source.is_file(),source
        destination=self.output/f'{label}.png';shutil.copy2(source,destination)
        return destination
