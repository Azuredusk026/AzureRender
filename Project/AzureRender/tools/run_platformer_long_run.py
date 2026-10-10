"""Observe and drive real-time platform routes through authenticated public input."""
import argparse
import json
from pathlib import Path
import secrets
import socket
import time
from run_playable_long_run import evaluate, run


def evaluate_platformer(data, formal=True):
    evaluate(data,formal=formal)
    count=data['completedRoutes']
    assert count >= (10 if formal else 1), 'Complete platform routes required'
    assert data['liveObservation'] and not data['controllerErrors'], 'Live route observation failed'
    assert data['mandatoryLandings']>=count*7, 'All mandatory landings required'
    assert data['courseCoins']==8, 'Main-route coin count must be eight'
    for key in ('startFallRecoveries','checkpointFallRecoveries','horizontalRides','verticalRides'):
        assert data[key]>=count, 'Missing route coverage: '+key


class PlatformerScenario:
    course='assets:/platformer/platformer.azurelevel'
    exploration='assets:/exploration.azurelevel'
    def __init__(self):
        self.phase='boot';self.keys=set();self.pulses={};self.phaseStarted=0
        self.counts=dict(completedRoutes=0,mandatoryLandings=0,startFallRecoveries=0,
            checkpointFallRecoveries=0,horizontalRides=0,verticalRides=0,restarts=0,switches=0)
        self.revisions={};self.errors=[];self.lastRecord=-1;self.courseCoins=0
    def prepare(self,package,output):
        design=package/'share/AzureRender/assets_public/platformer/PLATFORMER-DESIGN.json'
        assert (package/'game/assets/platformer/platformer.azurelevel').is_file(), 'Exploration successor package required'
        self.platforms=json.loads(design.read_text(encoding='utf-8'))['platforms']
        assert len(self.platforms)==8 and sum('motion' in p for p in self.platforms)==2
        self.output=output;self.endpoint=output/'endpoint.json';self.token=secrets.token_hex(32)
        self.journal=(output/'route.jsonl').open('w',encoding='utf-8')
        return []
    def arguments(self):
        return ['--validation-token-env','AZURE_PLATFORMER_LONG_RUN_TOKEN','--validation-endpoint',str(self.endpoint)]
    def environment(self):return {'AZURE_PLATFORMER_LONG_RUN_TOKEN':self.token}
    def call(self,op,**fields):
        request=dict(op=op,**fields,schemaVersion=1,token=self.token)
        started=time.perf_counter()
        self.connection.sendall((json.dumps(request)+'\n').encode())
        line=self.stream.readline(1024*1024+1)
        assert line and len(line)<=1024*1024,'Invalid live control response'
        response=json.loads(line)
        elapsed=time.perf_counter()-started
        if elapsed>.05:self.event('control-latency',operation=op,seconds=elapsed,engineElapsedMs=response.get('elapsedMs'))
        if not response['passed']:raise AssertionError(response)
        return response.get('value')
    def event(self,kind,**fields):
        self.journal.write(json.dumps(dict(kind=kind,**fields),ensure_ascii=False)+'\n');self.journal.flush()
    def input(self,**event):
        result=self.call('input',event=event)
        self.event('input',event=event,queued=result)
    def key(self,code,down):
        if (code in self.keys)==down:return
        self.input(action='key',key=code,down=down)
        if down:self.keys.add(code)
        else:self.keys.discard(code)
    def pulse(self,code,now):self.key(code,True);self.pulses[code]=now+.08
    def release(self):
        if hasattr(self,'address'):
            for code in list(self.keys):self.key(code,False)
        self.pulses.clear()
    def transition(self,phase,now):self.phase=phase;self.phaseStarted=now
    def load(self,reference,next_phase,state,now):
        self.release();self.desired=reference;self.nextPhase=next_phase
        self.oldRevision=state['levelRevision'];self.oldReference=state['levelReference']
        self.input(action='preload-level',reference=reference);self.transition('preloading',now)
    def begin_fall(self,checkpoint,state,now):
        self.release();self.checkpointFall=checkpoint;self.sawFall=False
        self.savedCoins=state['tasks'][0]['collected'];self.key(68,True)
        self.transition('fall-drive',now)
    def advance(self,state,now):
        if self.index==4 and not self.checkpointTested:
            assert state['checkpointActivated'];self.checkpointTested=True
            self.begin_fall(True,state,now)
        elif self.index==7:self.transition('finish',now)
        else:self.transition('walk',now)
    def update(self,now):
        try:self._update(now)
        except Exception as e:
            self.errors.append(dict(seconds=now,phase=self.phase,message=str(e)))
            self.event('controller-error',**self.errors[-1]);raise
    def _update(self,now):
        if self.phase=='boot' and now<11:return
        if not hasattr(self,'address'):
            if not self.endpoint.exists():
                assert now<45,'Validation endpoint startup timeout';return
            self.address=json.loads(self.endpoint.read_text(encoding='utf-8'))
            self.connection=socket.create_connection((self.address['address'],self.address['port']),timeout=3)
            self.connection.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1)
            self.stream=self.connection.makefile('rb')
        state=json.loads(self.call('query',name='game.state'))
        if now-self.lastRecord>=.1:
            self.event('state',seconds=now,value=state);self.lastRecord=now
        assert not state['scriptErrors'] and not state['presentationErrors'] and not state['loadError'],state
        for code,until in list(self.pulses.items()):
            if now>=until:self.key(code,False);del self.pulses[code]
        if not state['loading']:self.revisions[state['levelRevision']]=state['levelReference']
        assert now-self.phaseStarted<45,'Timed out in platform route phase: '+self.phase
        if self.phase=='boot':
            # Window recovery exercises complete at nine seconds. A resize can
            # block the native message loop; route control starts afterwards.
            if now>=11 and not state['loading']:self.load(self.course,'start',state,now)
            return
        if self.phase=='preloading':
            if not state['loading']:
                self.input(action='load-level',reference=self.desired);self.transition('loading',now)
            return
        if self.phase=='loading':
            if not state['loading'] and state['levelReference']==self.desired and state['levelRevision']>self.oldRevision:
                self.counts['switches']+=self.oldReference!=self.desired
                self.transition(self.nextPhase,now)
            return
        if self.phase=='return-course':self.load(self.course,'start',state,now);return
        if self.phase=='restart':
            if not state['loading'] and state['levelRevision']>self.oldRevision:
                self.counts['restarts']+=1;self.load(self.exploration,'return-course',state,now)
            return
        # Background asset scans keep the current world running. Scene
        # switches wait in the explicit preloading/loading phases above.
        assert state['levelReference']==self.course
        hero=next(c for c in state['characters'] if c['node']=='hero');position=hero['position']
        if self.phase=='start':
            self.index=0;self.checkpointTested=False;self.begin_fall(False,state,now);return
        if self.phase in ('fall-drive','fall-recover'):
            support_top=self.platforms[4 if self.checkpointFall else 0]['top']
            # Recovery can occur between network observations. A full metre
            # below the tested support proves the deliberately driven fall.
            if position[1]<support_top-1:self.sawFall=True
            if self.phase=='fall-drive' and now-self.phaseStarted>=1.4:
                self.key(68,False);self.transition('fall-recover',now)
            elif self.phase=='fall-recover':
                expected=self.platforms[4 if self.checkpointFall else 0]['z']
                if self.sawFall and hero['grounded'] and abs(position[0])<.4 and abs(position[2]-expected)<.4:
                    assert state['tasks'][0]['collected']==self.savedCoins,'Recovery must preserve coins'
                    key='checkpointFallRecoveries' if self.checkpointFall else 'startFallRecoveries'
                    self.counts[key]+=1;self.event('fall-recovered',checkpoint=self.checkpointFall,frame=state['frame'])
                    self.transition('walk',now)
            return
        if self.phase=='walk':
            self.key(87,True);source=self.platforms[self.index]
            if position[2]<=source['z']-source['halfLength']+.90:
                assert hero['grounded'],'Jump must start on a supported platform'
                self.pulse(32,now);self.transition('jump',now)
            return
        target=self.platforms[min(7,self.index+1)]
        if self.phase=='jump':
            # A completed goal disables movement before the controller's
            # stop point. Keep the existing grounded landing check.
            terminal_stop=self.index==6 and state['tasks'][0]['completed']
            if position[2]<=target['z']+.35 or terminal_stop:
                self.key(87,False);self.transition('land',now)
            return
        if self.phase=='land':
            if hero['grounded'] and abs(position[2]-target['z'])<.8 and position[1]>target['top']-.1:
                self.index+=1;self.counts['mandatoryLandings']+=1
                self.event('landing',platform=target['id'],frame=state['frame'],position=position)
                if 'motion' in target:
                    self.axis=0 if self.index==3 else 1
                    body=next(b for b in state['rigidBodies'] if b['node']==target['id'])
                    self.relative=position[self.axis]-body['position'][self.axis]
                    self.low=self.high=position[self.axis];self.transition('ride',now)
                else:self.advance(state,now)
            return
        if self.phase=='ride':
            platform=self.platforms[self.index]
            body=next(b for b in state['rigidBodies'] if b['node']==platform['id'])
            assert hero['grounded'] and abs(position[self.axis]-body['position'][self.axis]-self.relative)<.06,'Moving support must carry its rider'
            self.low=min(self.low,position[self.axis]);self.high=max(self.high,position[self.axis])
            if now-self.phaseStarted>=2.1:
                assert self.high-self.low>.05,'Moving platform must actually translate'
                self.counts['horizontalRides' if self.axis==0 else 'verticalRides']+=1
                self.advance(state,now)
            return
        if self.phase=='finish' and state['tasks'][0]['completed']:
            assert state['doorOpen'] and state['tasks'][0]['collected']==8,'Gate and coins must complete normally'
            assert state['audioStarts']>=15,'Route must play feedback sounds'
            self.courseCoins=8;self.counts['completedRoutes']+=1
            self.event('complete',frame=state['frame'],counts=self.counts)
            self.oldRevision=state['levelRevision'];self.pulse(82,now);self.transition('restart',now)
    def finalize(self,resources,warm_frame):
        groups={}
        for row in resources:
            if row['frame']<=warm_frame+240 or row['loading'] or row['committed']:continue
            reference=self.revisions.get(row['revision'])
            assert reference,'Resource sample lacks observed level identity'
            groups.setdefault(reference,[]).append(row)
        assert groups,'No stable equivalent-level resource samples'
        by_level={name:{key:max(r[key] for r in rows)/min(r[key] for r in rows)-1
            for key in ('buffers','images','bufferBytes','imageBytes')} for name,rows in groups.items()}
        return dict(**self.counts,controllerErrors=self.errors,liveObservation=True,courseCoins=self.courseCoins,
            sameLevelResidencyGrowth=by_level,revisionReferences=self.revisions,
            residencyGrowth={key:max(row[key] for row in by_level.values()) for key in next(iter(by_level.values()))})
    def close(self):
        if hasattr(self,'journal'):self.journal.close()
        if hasattr(self,'stream'):self.stream.close()
        if hasattr(self,'connection'):self.connection.close()
    def evaluate(self,data,formal):evaluate_platformer(data,formal=formal)


if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--package',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--seconds',type=float,default=1800)
    p.add_argument('--probe',action='store_true');a=p.parse_args()
    run(a.package,a.output,a.seconds,a.probe,scenario=PlatformerScenario())
