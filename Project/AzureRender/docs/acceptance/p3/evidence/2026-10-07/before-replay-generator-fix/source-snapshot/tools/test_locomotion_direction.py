"""Measure authored forward direction and Shift locomotion in the real Player."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import tempfile
from create_third_person_project import create


def scenario(player, root, name, *, character=None, keys=(87,), capture=False, fps=60, events=None, orbit=False):
    folder = root/name
    create(folder, character)
    level_path = folder/'assets/courtyard.azurelevel'
    level = json.loads(level_path.read_text(encoding='utf-8'))
    level['nodes'] = [n for n in level['nodes'] if n['id'] in ('hero', 'ground', 'camera')]
    ground = next(n for n in level['nodes'] if n['id']=='ground')['components']['azure.transform']['data']
    ground.update(translation=[0, -.25, 0], scale=[50, .25, 50])
    level_path.write_text(json.dumps(level, indent=2), encoding='utf-8')
    frames=fps*4
    if events is None:
        events = [{'frame':fps//2+1, 'action':'key', 'key':k, 'down':True} for k in keys]
        events += [{'frame':fps*3+1, 'action':'key', 'key':k, 'down':False} for k in keys]
    if orbit: events += [{'frame':fps//2+1,'action':'camera','x':600,'y':0,'scroll':0}]
    events.sort(key=lambda e:e['frame'])
    actions = root/(name+'.input.json')
    actions.write_text(json.dumps({'schemaVersion':1,'actions':events}), encoding='utf-8')
    report = root/(name+'.runtime.json')
    command = [str(player), '--project', str(folder/'project.azureproject'), '--width', '960', '--height', '540',
               '--game-actions', str(actions), '--runtime-report', str(report), '--capture-fps', str(fps)]
    if capture: command += ['--capture-dir', str(root/(name+'-capture')), '--capture-frames', str(frames)]
    else: command += ['--fixed-frame-step', '--smoke-frames', str(frames)]
    startup = subprocess.STARTUPINFO(); startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW; startup.wShowWindow = 0
    result = subprocess.run(command, cwd=player.parent, capture_output=True, encoding='utf-8', errors='replace', timeout=300, startupinfo=startup)
    (root/(name+'.log')).write_bytes((result.stdout+result.stderr).encode('utf-8'))
    assert result.returncode==0 and 'VUID-' not in result.stderr and 'Validation Error' not in result.stderr, result.stdout+result.stderr
    assert 'Allocator after unload: buffers=0 images=0' in result.stdout
    data=json.loads(report.read_text(encoding='utf-8'))
    assert not data['scriptErrors'] and not data['presentationErrors']
    heroes=[next(c for c in f['characters'] if c['node']=='hero') for f in data['routeFrames']]
    front_yaw=0 if character else 180
    dots=[]; speeds=[]
    for i in range(fps*3//2+1,fps*5//2+1):
        delta=[heroes[i]['position'][axis]-heroes[i-1]['position'][axis] for axis in (0,2)]
        length=math.hypot(*delta)
        speeds.append(length*fps)
        if length>=1e-6:
            angle=math.radians(heroes[i]['rotation'][1]+front_yaw)
            dots.append((math.sin(angle)*delta[0]+math.cos(angle)*delta[1])/length)
    return {'forwardDotMinimum':min(dots) if dots else 1, 'meanSpeed':sum(speeds)/len(speeds) if speeds else 0,
            'finalState':heroes[-1]['state'],'frames':len(heroes),'replayedActions':data['replayedActions'],
            'validationErrors':0, 'heroes':heroes, 'fixedSteps':data['fixedSteps']}


def run(player, root, character=None, capture=False):
    root.mkdir(parents=True,exist_ok=True)
    cases={'forward':{},'back':{'keys':(83,)},'left':{'keys':(65,)},'right':{'keys':(68,)},
           'diagonal':{'keys':(87,68)},'rotated':{'orbit':True},'sprint-left':{'keys':(87,340)},
           'sprint-right':{'keys':(87,344)},'sprint-diagonal':{'keys':(87,68,340)},
           'shift-alone':{'keys':(340,)},'fps30':{'keys':(87,344),'fps':30},'fps144':{'keys':(87,344),'fps':144}}
    results={};failures=[];fps_positions=[]
    for name,options in cases.items():
        result=scenario(player,root,name,character=character,capture=capture and name=='forward',**options)
        heroes=result.pop('heroes')
        expected=0 if name=='shift-alone' else 5 if ('sprint' in name or name.startswith('fps')) else 2
        result['passed']=result['forwardDotMinimum']>=.95 and abs(result['meanSpeed']-expected)<=max(.001,expected*.02) and result['finalState']=='idle'
        rates=[h['playbackRate'] for h in heroes[91:151]] if name in ('forward','sprint-left') else []
        if rates:result['passed'] &= max(abs(rate-expected/2) for rate in rates)<.02
        if name in ('sprint-right','fps30','fps144'):fps_positions.append(heroes[-1]['position'])
        results[name]=result
        if not result['passed']:failures.append(name)
    def action(frame,key,down):return {'frame':frame,'action':'key','key':key,'down':down}
    events=[action(31,87,True),action(31,340,True),action(90,344,True),action(100,340,False),action(150,344,False),action(181,87,False)]
    result=scenario(player,root,'both-release',character=character,events=events)
    heroes=result.pop('heroes')
    def measured(a,b):return math.hypot(heroes[b]['position'][0]-heroes[a]['position'][0],heroes[b]['position'][2]-heroes[a]['position'][2])*60/(b-a)
    result['heldSpeed']=measured(110,140);result['releasedSpeed']=measured(165,179)
    result['passed']=abs(result['heldSpeed']-5)<=.1 and abs(result['releasedSpeed']-2)<=.04
    results['both-release']=result
    if not result['passed']:failures.append('both-release')
    events=[action(31,87,True),action(31,340,True),{'frame':90,'action':'focus','focused':False},{'frame':120,'action':'focus','focused':True}]
    result=scenario(player,root,'focus',character=character,events=events);heroes=result.pop('heroes')
    result['passed']=heroes[120]['position']==heroes[-1]['position'] and result['finalState']=='idle'
    results['focus']=result
    if not result['passed']:failures.append('focus')
    maximum_position_difference=max(abs(p[axis]-fps_positions[0][axis]) for p in fps_positions for axis in range(3))
    if maximum_position_difference>1e-5:failures.append('fixed-step final position')
    summary={'passed':not failures,'cases':results,'fpsPositionDifference':maximum_position_difference,'failures':failures,'validationErrors':0}
    (root/'summary.json').write_bytes((json.dumps(summary,indent=2)+'\n').encode())
    print(json.dumps(summary),flush=True)
    assert not failures,failures


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--player',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--character',type=Path)
    parser.add_argument('--capture',action='store_true')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure_locomotion_') as temporary:
        run(args.player.resolve(),args.output.resolve() if args.output else Path(temporary),args.character,args.capture)
