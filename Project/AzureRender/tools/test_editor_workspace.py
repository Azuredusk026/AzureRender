"""Exercise real Dear ImGui hit testing, text focus, docking and DPI."""
import argparse
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile
from create_playable_project import create


def launch(executable, root, name, size, dpi, actions=(), capture=True, config=None, close_policy="save"):
    folder=root/name;folder.mkdir(parents=True)
    config=config or folder/'config'
    project=root/'game/project.azureproject'
    task=folder/'ui-input.json';task.write_text(json.dumps(actions,ensure_ascii=False),encoding='utf-8')
    env=os.environ.copy();env['AZURERENDER_EDITOR_CONFIG']=str(config)
    env['AZURERENDER_EDITOR_UI_ACTIONS']=str(task);env['AZURERENDER_EDITOR_DPI']=str(dpi)
    report=folder/'runtime.json'
    count=max([16]+[a['frame']+8 for a in actions])
    command=[str(executable),'--editor-close-policy', close_policy, '--editor-project',str(project),'--width',str(size[0]),'--height',str(size[1]),
             '--runtime-report',str(report),'--fixed-frame-step']
    command += ['--capture-dir',str(folder/'capture'),'--capture-frames',str(count)] if capture else ['--smoke-frames',str(count)]
    info=subprocess.STARTUPINFO();info.dwFlags|=subprocess.STARTF_USESHOWWINDOW;info.wShowWindow=0
    result=subprocess.run(command,cwd=folder,env=env,capture_output=True,encoding='utf-8',errors='replace',timeout=180,startupinfo=info)
    (folder/'stdout.log').write_bytes(result.stdout.encode());(folder/'stderr.log').write_bytes(result.stderr.encode())
    assert result.returncode==0,result.stdout+result.stderr
    assert 'VUID-' not in result.stderr and 'Validation Error' not in result.stderr
    assert 'Allocator after unload: buffers=0 images=0' in result.stdout
    data=json.loads(report.read_text(encoding='utf-8'))['editorWorkspace']
    assert data['version']==2 and set(data['panels'])=={'viewport','outliner','inspector','assets','capture','console','build','animation','gameplay-debug','settings'},data
    assert data['image']['width']>=480 and data['image']['height']>=270,data['image']
    assert len(data['panels'])==10
    assert data['uiErrors']==[],data['uiErrors']
    return data


def main(executable,root):
    root.mkdir(parents=True,exist_ok=True);create(root/'game')
    results={}
    hitErrors=[]
    cases=[(dpi,size) for dpi in (1,1.5,2) for size in ((1920,1080),(1280,720))]
    cases += [(.75,(1280,720)),(3,(3840,2160))]
    for dpi,size in cases:
        name=f'{size[0]}x{size[1]}-{dpi}'
        results[name]=launch(executable,root,name,size,dpi)
        assert all(p['docked'] for key,p in results[name]['panels'].items() if key!='settings')
        # Independent pinhole calculation for the initial focused hero.
        imageRect=results[name]['image']
        position=results[name]['camera']['position'];target=results[name]['camera']['target']
        def normalize(vector):
            length=math.sqrt(sum(value*value for value in vector));return [value/length for value in vector]
        forward=normalize([b-a for a,b in zip(position,target)])
        right=normalize([-forward[2],0,forward[0]])
        up=[right[1]*forward[2]-right[2]*forward[1],right[2]*forward[0]-right[0]*forward[2],right[0]*forward[1]-right[1]*forward[0]]
        relative=[-value for value in position]
        depth=sum(a*b for a,b in zip(relative,forward))
        tangent=math.tan(math.pi/6)/1.6
        expected=(imageRect['x']+imageRect['width']*(1+sum(a*b for a,b in zip(relative,right))/(depth*tangent*imageRect['width']/imageRect['height']))/2,
                  imageRect['y']+imageRect['height']*(1-sum(a*b for a,b in zip(relative,up))/(depth*tangent))/2)
        rect=results[name]['widgets']['gizmo.center']
        error=math.hypot(rect[0]+rect[2]/2-expected[0],rect[1]+rect[3]/2-expected[1])
        assert error<=2,(name,error)
        from PIL import Image
        pixels=Image.open(root/name/'capture/frame_000015.png').convert('RGB')
        green=(expected[0],expected[1]-40*dpi)
        matches=[]
        for y in range(round(green[1])-2,round(green[1])+3):
            for x in range(round(green[0])-2,round(green[0])+3):
                if max(abs(a-b) for a,b in zip(pixels.getpixel((x,y)),(80,220,110)))<=5:matches.append((x,y))
        assert matches,(name,'Rendered Y handle missing at the projected mouse target')
        rasterError=min(math.hypot(x-green[0],y-green[1]) for x,y in matches)
        assert rasterError<=2,(name,rasterError)
        hitErrors.append(dict(case=name,projectionErrorPixels=error,rasterErrorPixels=rasterError))
        if dpi==1 and size[0]==1920:
            from PIL import Image
            color=Image.open(root/name/'capture/frame_000015.png').convert('RGB').getpixel((110,700))
            assert max(abs(a-b) for a,b in zip(color,(32,35,41)))<=2,color
    actions=[]
    def click(frame,target):actions.append(dict(frame=frame,action='click',target=target))
    def key(frame,key,down=True,ctrl=False):actions.append(dict(frame=frame,action='key',key=key,down=down,ctrl=ctrl))
    click(25,'node.hero:body');click(30,'name');key(33,'A',ctrl=True);key(34,'A',False,True);key(35,'A',False,False)
    actions.append(dict(frame=36,action='text',text='中文主角'))
    key(39,'Enter');key(40,'Enter',False);click(45,'save')
    click(50,'play');click(55,'pause');click(60,'step');click(65,'resume');click(70,'stop')
    click(75,'menu.View');click(79,'panel.console')
    data=launch(executable,root,'controls',(1920,1080),1,actions,capture=False)
    assert data['selectedName']=='中文主角',data
    states={r['frame']:r for r in data['history']}
    assert states[52]['playing'] and states[57]['paused'] and not states[72]['playing']
    assert not data['panels']['console']['open']
    config=root/'controls/config'
    restored=launch(executable,root,'reopened',(1920,1080),1,capture=False,config=config)
    assert not restored['panels']['console']['open'] and restored['selectedName']=='中文主角'
    (config/'workspace.json').write_text('{broken',encoding='utf-8')
    recovered=launch(executable,root,'corrupt',(1920,1080),1,config=config)
    assert recovered['panels']['console']['open'] and recovered['diagnostic']
    iniConfig=root/'reopened/config-copy';iniConfig.mkdir()
    import shutil
    for filename in ('workspace.json','layout.ini'):shutil.copy2(root/'reopened/config'/filename if (root/'reopened/config'/filename).exists() else config/filename,iniConfig/filename)
    # Save a valid configuration first, then corrupt only its docking bytes.
    launch(executable,root,'ini-valid',(1920,1080),1,capture=False,config=iniConfig)
    with (iniConfig/'layout.ini').open('a',encoding='utf-8') as file:file.write('\n[Docking][Data]\nmalformed\n')
    iniRecovered=launch(executable,root,'ini-corrupt',(1920,1080),1,capture=False,config=iniConfig)
    assert iniRecovered['diagnostic'] and all(p['docked'] for key,p in iniRecovered['panels'].items() if key!='settings')
    focus=[dict(frame=25,action='click',target='name')]
    for index,(value,ctrl) in enumerate((('W',False),('A',False),('S',False),('D',False),('Shift',False),('D',True),('Z',True),('Y',True),('S',True),('Delete',False))):
        frame=28+index*3
        focus.extend([dict(frame=frame,action='key',key=value,down=True,ctrl=ctrl),dict(frame=frame+1,action='key',key=value,down=False,ctrl=False)])
    savedLevels={str(path):path.read_bytes() for path in (root/'game').rglob('*.azurelevel')}
    focused=launch(executable,root,'text-focus',(1920,1080),1,focus,capture=False)
    assert all(Path(path).read_bytes()==value for path,value in savedLevels.items()),'Text focus Ctrl+S must not save scene'
    states=focused['history'];assert all(state['nodeCount']==states[0]['nodeCount'] and state['translation']==states[0]['translation'] and not state['playing'] for state in states)
    dynamic=launch(executable,root,'dynamic-dpi',(1280,720),1,[dict(frame=25,action='dpi',scale=2),dict(frame=35,action='dpi',scale=1.5)])
    assert dynamic['dpi']==1.5 and all(p['docked'] for key,p in dynamic['panels'].items() if key!='settings')
    reset=launch(executable,root,'reset',(1920,1080),1,[dict(frame=25,action='click',target='menu.View'),dict(frame=29,action='click',target='menu.Reset Layout')],capture=False,config=config)
    assert all(p['open'] and p['docked'] for key,p in reset['panels'].items() if key!='settings')
    for mode,field,baseline in (('Move','gizmoTranslation',0),('Rotate','gizmoRotation',0),('Scale','gizmoScale',1)):
        events=[dict(frame=25,action='click',target='node.hero:body'),dict(frame=28,action='click',target='focus'),dict(frame=30,action='click',target='mode.'+mode),
            dict(frame=35,action='mouse',target='gizmo.0',down=True),dict(frame=38,action='mouse',target='gizmo.0',down=True,offset=[30,0]),dict(frame=41,action='mouse',target='gizmo.0',down=False,offset=[30,0])]
        manipulated=launch(executable,root,'gizmo-'+mode,(1920,1080),1,events,capture=False)
        assert manipulated[field][0]>baseline,manipulated[field]
    scripts=launch(executable,root,'asset-scripts',(1920,1080),1,[dict(frame=25,action='click',target='tool.assets'),dict(frame=29,action='click',target='assets.type'),dict(frame=33,action='click',target='type.Scripts')])
    assert scripts['visibleAssets'] and all(asset['type']=='.lua' for asset in scripts['visibleAssets'])
    picking=launch(executable,root,'picking',(1920,1080),1,[dict(frame=25,action='click',target='node.hero:body'),dict(frame=28,action='click',target='focus'),dict(frame=32,action='click',target='node.ground'),dict(frame=36,action='click',target='pick.hero:body')],capture=False)
    assert picking['history'][-1]['selected']=='hero:body',picking['history'][-1]
    drag=launch(executable,root,'dock-drag',(1920,1080),1,[dict(frame=23,action='mouse',target='tab.assets',down=False),dict(frame=25,action='mouse',target='tab.assets',down=True),dict(frame=29,action='mouse',target='panelrect.outliner',down=True),dict(frame=33,action='mouse',target='panelrect.outliner',down=True),dict(frame=37,action='mouse',target='panelrect.outliner',down=False)],capture=False)
    assert drag['panels']['assets']['docked'] and drag['panels']['assets']['rect'][1]<500,drag['panels']['assets']
    dragReload=launch(executable,root,'dock-reopened',(1920,1080),1,capture=False,config=root/'dock-drag/config')
    assert dragReload['panels']['assets']['rect']==drag['panels']['assets']['rect']
    summary=dict(passed=True,dpiCases=len(results),realImGuiEvents=len(actions),
                 chineseSaveReload=True,runControls=True,panelPersistence=True,corruptionRecovery=True,
                 iniIntegrity=True,textFocusProtection=True,dynamicDpi=True,layoutReset=True,gizmoModes=3,scriptAssets=True,viewportPicking=True,dockDragPersistence=True,hitErrors=hitErrors)
    (root/'summary.json').write_bytes((json.dumps(summary,indent=2)+'\n').encode());print(json.dumps(summary))


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure_workspace_') as temporary:
        main(args.executable.resolve(),args.output.resolve() if args.output else Path(temporary))
