"""Optional model service: bounded JSON-RPC over NDJSON standard streams."""
import argparse
import json
import os
from pathlib import Path
import sys
import threading
import time
from ai.profile import profiles, resolve_profile
from ai.session import SessionRegistry

FRAME_LIMIT=8*1024*1024
SOURCE_LIMIT=2*1024*1024

def decode(raw):
    return json.loads(raw,parse_constant=lambda text: (_ for _ in ()).throw(ValueError('Nonfinite JSON')))

class Bridge:
    def __init__(self,fixture=None,config=None):
        self.fixture=fixture
        self.config=config or {'schemaVersion':1,'providers':[],'profiles':[]}
        self.initialized=False
        self.writer=threading.Lock()
        self.lock=threading.Lock()
        self.runs={}
        self.sessions=SessionRegistry()
        self.next_response=0
        self.stopping=False
    def write(self,row):
        raw=json.dumps(row,ensure_ascii=False,allow_nan=False,separators=(',',':')).encode('utf-8')
        if len(raw)>FRAME_LIMIT:raise ValueError('Response frame budget exceeded')
        with self.writer:
            sys.stdout.buffer.write(raw+b'\n');sys.stdout.buffer.flush()
    def result(self,identity,value):
        self.write(dict(jsonrpc='2.0',id=identity,result=value))
    def error(self,identity,code,message):
        self.write(dict(jsonrpc='2.0',id=identity,error=dict(code=code,message=message)))
    def providers(self):
        if self.fixture is not None:
            return [dict(id='fixed',kind='fixture',configured=True,available=True)]
        return [dict(id=row['id'],kind=row['kind'],configured=not row['credentialEnv'] or bool(os.environ.get(row['credentialEnv'])),available=True) for row in self.config['providers']]
    def profiles(self):
        return profiles(self.config,self.fixture is not None)
    def chat(self,identity,params):
        from ai.protocol import validate_chat
        validate_chat(params)
        profile=resolve_profile(self.profiles(),params['profile'])
        run=params['runId'];event=threading.Event()
        with self.lock:
            if self.runs:raise ValueError('Service already owns an active request')
            response_index=self.next_response;self.next_response+=1
            self.runs[run]=event
        def worker():
            deadline=time.monotonic()+params['timeoutMs']/1000
            def check():
                if event.is_set():raise InterruptedError('Cancelled')
                if time.monotonic()>=deadline:raise TimeoutError('Timeout')
            try:
                if self.fixture is not None:
                    delay=self.fixture.get('delayMs',0)/1000
                    start=time.monotonic()
                    while time.monotonic()-start<delay:
                        check();event.wait(min(.01,delay))
                    check()
                    responses=self.fixture['responses']
                    selected=self.fixture.get('runs',{}).get(run,responses[min(response_index,len(responses)-1)])
                    content=selected if isinstance(selected,str) else json.dumps(selected,allow_nan=False,separators=(',',':'))
                    mode='json'
                else:
                    from ai.provider import complete
                    content,mode=complete(self.config,profile,params,check,deadline)
                if len(content.encode('utf-8'))>SOURCE_LIMIT:raise ValueError('Model source budget exceeded')
                for sequence,offset in enumerate(range(0,len(content),65536),1):
                    check()
                    self.write(dict(jsonrpc='2.0',method='run.event',params=dict(runId=run,sequence=sequence,delta=content[offset:offset+65536])))
                check()
                self.result(identity,dict(runId=run,content=content,structuredMode=mode))
            except InterruptedError:self.error(identity,'Cancelled','Request cancelled')
            except TimeoutError:self.error(identity,'Timeout','Request deadline exceeded')
            except Exception:
                self.error(identity,'ProviderFailure','Provider request or bounded result failed')
            finally:
                with self.lock:self.runs.pop(run,None)
        thread=threading.Thread(target=worker,daemon=True)
        with self.lock:self.runs[run]=(event,thread)
        thread.start()
    def handle(self,frame):
        identity=frame.get('id') if isinstance(frame,dict) else None
        try:
            if not isinstance(frame,dict) or set(frame)!={'jsonrpc','id','method','params'} or frame['jsonrpc']!='2.0' or type(identity) is not int or identity<1:
                raise ValueError('Invalid JSON-RPC envelope')
            method=frame['method'];params=frame['params']
            if not isinstance(method,str) or not isinstance(params,dict):raise ValueError('Invalid method parameters')
            if method=='initialize':
                if set(params)!={'protocolVersion'} or type(params['protocolVersion']) is not int or params['protocolVersion']!=1:
                    raise ValueError('Unsupported protocol version')
                self.initialized=True
                self.result(identity,dict(protocolVersion=1,capabilities=['stateless','stream','cancel','sessions']))
                return
            if not self.initialized:raise ValueError('Initialize the service first')
            if method=='llm.chat':self.chat(identity,params)
            elif method in ('providers.list','profiles.list'):
                if params:raise ValueError('Catalog parameters must be empty')
                self.result(identity,self.providers() if method=='providers.list' else self.profiles())
            elif method=='run.cancel':
                if set(params)!={'runId'} or not isinstance(params['runId'],str):raise ValueError('Invalid cancellation')
                with self.lock:
                    item=self.runs.get(params['runId'])
                    if item:item[0].set()
                self.result(identity,dict(cancelled=item is not None))
            elif method=='session.create':
                if set(params)!={'profile'}:raise ValueError('Invalid session profile')
                key=self.sessions.create(self.profiles(),params['profile'])
                self.result(identity,dict(sessionId=key,stateless=True))
            elif method=='session.close':
                if set(params)!={'sessionId'}:raise ValueError('Unknown session')
                self.sessions.close(params['sessionId']);self.result(identity,dict(closed=True))
            elif method=='shutdown':
                if params:raise ValueError('Shutdown parameters must be empty')
                self.stopping=True;self.stop();self.result(identity,dict(stopped=True))
            else:raise ValueError('Unknown service method')
        except (ValueError,KeyError,TypeError) as error:self.error(identity,'InvalidRequest',str(error))
    def stop(self):
        self.sessions.clear()
        with self.lock:items=list(self.runs.values())
        for event,thread in items:event.set()
        for event,thread in items:thread.join(timeout=1)

def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--fixture',type=Path);parser.add_argument('--config',type=Path)
    args=parser.parse_args()
    if args.fixture and args.config:parser.error('Choose a fixture or a provider configuration')
    fixture=None;config=None
    if args.fixture:
        if args.fixture.stat().st_size>SOURCE_LIMIT:raise ValueError('Fixture exceeds source budget')
        fixture=decode(args.fixture.read_text(encoding='utf-8'))
        if not isinstance(fixture,dict) or not fixture.get('responses') or len(fixture['responses'])>24:raise ValueError('Invalid fixed responses')
        if type(fixture.get('delayMs',0)) is not int or not 0<=fixture.get('delayMs',0)<=300000:raise ValueError('Invalid fixture delay')
    if args.config:
        from ai.provider import load_config
        config=load_config(args.config)
    bridge=Bridge(fixture,config)
    try:
        while not bridge.stopping:
            raw=sys.stdin.buffer.readline(FRAME_LIMIT+2)
            if not raw:break
            if len(raw)>FRAME_LIMIT:
                while raw and not raw.endswith(b'\n'):raw=sys.stdin.buffer.readline(FRAME_LIMIT+2)
                bridge.error(None,'FrameBudget','Input frame exceeds budget');continue
            try:bridge.handle(decode(raw))
            except (ValueError,UnicodeDecodeError):bridge.error(None,'ParseError','Invalid JSON frame')
    finally:bridge.stop()

if __name__=='__main__':main()
