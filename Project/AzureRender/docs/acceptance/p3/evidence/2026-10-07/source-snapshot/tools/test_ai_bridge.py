"""Exercise the real NDJSON bridge, cancellation and provider isolation."""
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT=Path(__file__).resolve().parents[1]
BRIDGE=ROOT/'tools/ai_bridge.py'

class Peer:
    def __init__(self, fixture=None, config=None, env=None):
        self.temp=tempfile.TemporaryDirectory(prefix='azure model bridge ')
        path=Path(self.temp.name)/'fixture.json'
        path.write_text(json.dumps(config if config is not None else fixture),encoding='utf-8')
        self.process=subprocess.Popen([sys.executable,str(BRIDGE),'--config' if config is not None else '--fixture',str(path)],
            stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8',env=env)
        self.rows=queue.Queue()
        def read():
            for line in self.process.stdout:
                self.rows.put(json.loads(line))
            self.rows.put(None)
        self.reader=threading.Thread(target=read,daemon=True);self.reader.start()
        self.sequence=0
    def send(self,method,params=None):
        self.sequence+=1
        self.process.stdin.write(json.dumps(dict(jsonrpc='2.0',id=self.sequence,method=method,params=params or {}))+'\n')
        self.process.stdin.flush()
        return self.sequence
    def response(self,identity):
        events=[]
        for _ in range(100):
            row=self.rows.get(timeout=5)
            if row is None:
                raise AssertionError('Bridge exited: '+self.process.stderr.read())
            if row.get('id')==identity:return row,events
            events.append(row)
        raise AssertionError('Response exceeds frame budget')
    def call(self,method,params=None):
        return self.response(self.send(method,params))
    def initialize(self):
        result,_=self.call('initialize',dict(protocolVersion=1))
        assert result['result']['protocolVersion']==1,result
    def close(self):
        if self.process.poll() is None:
            self.call('shutdown')
            self.process.wait(timeout=5)
        self.process.stdin.close();self.reader.join(timeout=5)
        self.process.stdout.close();self.process.stderr.close();self.temp.cleanup()

def request(run='run-1',**fields):
    return dict(runId=run,prompt='Build a valid candidate',schema={'type':'object'},
                history=[],profile='content',timeoutMs=5000,**fields)

class BridgeTests(unittest.TestCase):
    def test_shared_request_fixture(self):
        from ai.protocol import validate_chat
        fixtures=json.loads((ROOT/'tests/fixtures/ai/protocol.json').read_text(encoding='utf-8'))
        for row in fixtures['requests']:
            data=request();data.update(row['overrides'])
            if row['valid']:validate_chat(data)
            else:
                with self.assertRaises(ValueError):validate_chat(data)
    def test_http_provider_routes_structured_modes_and_hides_failures(self):
        received=[]
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*args):pass
            def do_POST(self):
                payload=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
                received.append((payload,self.headers.get('Authorization')))
                if payload['messages'][-1]['content']=='fail':
                    self.send_response(500);self.end_headers();self.wfile.write(b'private-test-token');return
                raw=json.dumps({'choices':[{'message':{'content':'{"schemaVersion":1}'}}]}).encode()
                self.send_response(200);self.end_headers();self.wfile.write(raw)
        server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        self.addCleanup(server.server_close);self.addCleanup(server.shutdown)
        for mode in ('native','json','prompt'):
            config=dict(schemaVersion=1,providers=[dict(id='local',kind='chat-completions',
                endpoint=f'http://127.0.0.1:{server.server_port}/chat',credentialEnv='AZURE_TEST_PROVIDER_KEY',model='fixture',structuredMode=mode)],
                profiles=[dict(id='content',provider='local',systemPrompt='Build valid content')])
            p=Peer(config=config,env=dict(os.environ,AZURE_TEST_PROVIDER_KEY='private-test-token'));self.addCleanup(p.close);p.initialize()
            result,events=p.call('llm.chat',request());self.assertEqual(result['result']['structuredMode'],mode)
            self.assertTrue(events);payload,auth=received[-1];self.assertEqual(auth,'Bearer private-test-token')
            if mode=='native':self.assertEqual(payload['response_format']['json_schema']['schema'],{'type':'object'})
            elif mode=='json':self.assertEqual(payload['response_format'],{'type':'json_object'})
            else:self.assertIn('matching this schema',payload['messages'][0]['content']);self.assertNotIn('response_format',payload)
            data=request('failed');data['prompt']='fail'
            failure,_=p.call('llm.chat',data)
            self.assertEqual(failure['error']['code'],'ProviderFailure');self.assertNotIn('private-test-token',json.dumps(failure))
    def test_session_registry_limits_and_profile_ownership(self):
        from ai.session import SessionRegistry
        from ai.profile import profiles, resolve_profile
        catalog=profiles({'profiles':[{'id':'scene','provider':'local','systemPrompt':'Build'}]},False)
        self.assertEqual(resolve_profile(catalog,'scene')['provider'],'local')
        with self.assertRaises(ValueError):resolve_profile(catalog,'missing')
        sessions=SessionRegistry()
        keys=[sessions.create(catalog,'scene') for _ in range(64)]
        with self.assertRaises(ValueError):sessions.create(catalog,'scene')
        with self.assertRaises(ValueError):sessions.close('missing')
        sessions.close(keys[0]);self.assertTrue(sessions.create(catalog,'scene'))
        sessions.clear();self.assertTrue(sessions.create(catalog,'scene'))
    def peer(self,fixture=None):
        peer=Peer(fixture or dict(responses=[{'schemaVersion':1,'operations':[]}]))
        self.addCleanup(peer.close)
        return peer
    def test_handshake_catalog_session_and_stream(self):
        p=self.peer();p.initialize()
        providers,_=p.call('providers.list');self.assertEqual(providers['result'][0]['id'],'fixed')
        profiles,_=p.call('profiles.list');self.assertTrue(any(row['id']=='content' for row in profiles['result']))
        session,_=p.call('session.create',{'profile':'content'});identity=session['result']['sessionId']
        result,events=p.call('llm.chat',request())
        self.assertEqual(json.loads(result['result']['content'])['schemaVersion'],1)
        self.assertEqual(result['result']['structuredMode'],'json')
        self.assertTrue(events)
        self.assertEqual([r['params']['sequence'] for r in events],list(range(1,len(events)+1)))
        self.assertTrue(all(r['params']['runId']=='run-1' for r in events))
        self.assertIn('result',p.call('session.close',{'sessionId':identity})[0])
    def test_version_and_uninitialized_rejection(self):
        p=self.peer()
        self.assertIn('error',p.call('llm.chat',request())[0])
        for version in (2,True,1.0):
            self.assertIn('error',p.call('initialize',dict(protocolVersion=version))[0])
        p.initialize()
        self.assertIn('error',p.call('unknown.method')[0])
    def test_timeout_and_cancel_keep_service_available(self):
        p=self.peer(dict(responses=[{'ok':True}],delayMs=2000));p.initialize()
        slow=request();slow['timeoutMs']=20
        result,_=p.call('llm.chat',slow);self.assertEqual(result['error']['code'],'Timeout')
        pending=p.send('llm.chat',request('cancel-me'))
        time.sleep(.05)
        cancel=p.send('run.cancel',dict(runId='cancel-me'))
        # The cancelled request and acknowledgement can arrive in either order.
        responses={}
        while len(responses)<2:
            row=p.rows.get(timeout=5)
            if 'id' in row:responses[row['id']]=row
        self.assertEqual(responses[pending]['error']['code'],'Cancelled')
        self.assertTrue(responses[cancel]['result']['cancelled'])
        self.assertIn('result',p.call('providers.list')[0])
    def test_budget_invalid_fields_and_stateless_history(self):
        p=self.peer();p.initialize()
        for field,value in [('timeoutMs',0),('timeoutMs',300001),('timeoutMs',True),('history',[{}]*25),
                             ('prompt','x'*(2097152+1)),('profile','unknown')]:
            data=request();data[field]=value
            self.assertIn('error',p.call('llm.chat',data)[0],field)
        data=request();data['shell']='arbitrary command'
        self.assertIn('error',p.call('llm.chat',data)[0])
        self.assertIn('result',p.call('llm.chat',request('valid'))[0])

if __name__=='__main__':
    unittest.main()
