"""Exercise held RMB navigation through production ImGui inputs."""
import argparse,json,shutil,math,uuid
from pathlib import Path
from test_editor_workspace import launch

def run(executable,output):
    output=output/('run-'+uuid.uuid4().hex[:8]);output.mkdir(parents=True)
    shutil.copytree(Path(__file__).resolve().parents[1]/'assets_public/scene_inspector',output/'game',ignore=shutil.ignore_patterns('.azure'))
    baseline=launch(executable,output,'baseline',(1280,720),1,capture=False)
    held=[dict(frame=21,action='mouse',target='viewport.image',button='right',down=False),dict(frame=25,action='mouse',target='viewport.image',button='right',down=True)]
    for key in ('D','S','Z','Y'):
        modified=launch(executable,output,'held-control-'+key,(1280,720),1,held+[
            dict(frame=30,action='key',key=key,down=True,ctrl=True),
            dict(frame=31,action='key',key=key,down=False,ctrl=True),
            dict(frame=32,action='key',key=key,down=False,ctrl=False),
            dict(frame=35,action='mouse',target='viewport.image',button='right',down=False)],capture=False)
        assert modified['undoCount']==baseline['undoCount'],'Navigation must preserve document history'
        assert modified['feedback']==[],'Navigation must not route document shortcuts'
    actions=held+[dict(frame=30,action='wheel',y=2),dict(frame=36,action='key',key='Escape',down=True),dict(frame=37,action='key',key='Escape',down=False),dict(frame=40,action='mouse',target='viewport.image',button='right',down=False)]
    wheel=launch(executable,output,'flight-wheel',(1280,720),1,actions,capture=False)
    assert wheel['camera']==baseline['camera'],'Held RMB wheel must adjust speed without dollying camera'
    assert wheel['navigation']['speed']>5 and wheel['navigation']['mode']=='idle',wheel.get('navigation')
    assert not wheel['navigation']['cursorCaptured'] and wheel['capture']['navigationButton']==-1
    assert wheel['navigation']['nativeCursorMode']=='normal' and not wheel['navigation']['rawMouse']
    movements={}
    for name,key,axis,sign in [('forward','W',2,-1),('backward','S',2,1),('right','D',0,1),('left','A',0,-1),('up','E',1,1),('down','Q',1,-1)]:
        events=held+[dict(frame=28,action='key',key=key,down=True),dict(frame=38,action='key',key=key,down=False),dict(frame=40,action='mouse',target='viewport.image',button='right',down=False)]
        result=launch(executable,output,name,(1280,720),1,events,capture=False)
        displacement=result['camera']['position'][axis]-baseline['camera']['position'][axis]
        assert displacement*sign>0,(name,displacement)
        assert not result['navigation']['cursorCaptured']
        assert result['navigation']['nativeCursorMode']=='normal'
        movements[name]=abs(displacement)
    boosted=launch(executable,output,'boost',(1280,720),1,held+[
        dict(frame=27,action='key',key='Shift',down=True,shift=True),dict(frame=28,action='key',key='W',down=True,shift=True),
        dict(frame=38,action='key',key='W',down=False,shift=True),dict(frame=39,action='key',key='Shift',down=False),
        dict(frame=40,action='mouse',target='viewport.image',button='right',down=False)],capture=False)
    ratio=abs(boosted['camera']['position'][2]-baseline['camera']['position'][2])/movements['forward']
    assert math.isclose(ratio,4,rel_tol=.01),ratio
    # A stationary right click opens a context menu, which owns keyboard input.
    context=launch(executable,output,'context-tool-blocked',(1280,720),1,held+[
        dict(frame=30,action='mouse',target='viewport.image',button='right',down=False),
        dict(frame=34,action='key',key='E',down=True),dict(frame=35,action='key',key='E',down=False)],capture=False)
    assert 'viewport.duplicate' in context['widgets']
    assert context['gizmoMode']==0 and context['camera']==baseline['camera']
    for key,mode in [('W',0),('E',1),('R',2)]:
        result=launch(executable,output,'tool-'+key,(1280,720),1,held+[
            dict(frame=30,action='mouse',target='viewport.image',button='right',down=False),
            dict(frame=31,action='key',key='Escape',down=True),dict(frame=32,action='key',key='Escape',down=False),
            dict(frame=34,action='key',key=key,down=True),dict(frame=35,action='key',key=key,down=False)],capture=False)
        assert result['gizmoMode']==mode and result['camera']==baseline['camera'],(key,result['gizmoMode'])
    for name,exit_events in [
        ('focus',[dict(frame=34,action='focus',focused=False)]),
        ('popup',[dict(frame=34,action='click',target='menu.View')]),
        ('dpi',[dict(frame=32,action='dpi',scale=1.5),dict(frame=40,action='mouse',target='viewport.image',button='right',down=False)]),
        ('outside',[dict(frame=32,action='mouse',target='panelrect.console',button='right',down=True),dict(frame=40,action='mouse',target='panelrect.console',button='right',down=False)])]:
        result=launch(executable,output,'exit-'+name,(1280,720),1,held+exit_events,capture=False)
        assert not result['navigation']['cursorCaptured'] and result['navigation']['nativeCursorMode']=='normal',name
        assert result['uiErrors']==[]
    orientations=[]
    axis_click=launch(executable,output,'direction-control',(1280,720),1,[
        dict(frame=25,action='click',target='viewport.axis.+X')],capture=False)
    assert axis_click['view']['projection']=='orthographic',axis_click['view']
    assert axis_click['view']['position'][0]>axis_click['view']['target'][0]
    for axis in ['+X','-X','+Y','-Y','+Z','-Z']:
        events=[dict(frame=25,action='click',target='viewport.view-options'),dict(frame=30,action='click',target='viewport.orientation.'+axis),dict(frame=35,action='click',target='node.asset-a'),dict(frame=40,action='key',key='F',down=True),dict(frame=41,action='key',key='F',down=False),dict(frame=48,action='click',target='pick.asset-a')]
        result=launch(executable,output,'axis-'+axis.replace('+','positive').replace('-','negative'),(1280,720),1,events)
        assert result['view']['projection']=='orthographic' and result['activeSelection']=='asset-a',(axis,result.get('view'),result['activeSelection'])
        assert result['view']['orthoHeight']>0 and result['feedback']==[],result['feedback']
        orientations.append(axis)
    summary={'passed':True,'heldWheelAdjustsSpeed':True,'sixDirections':True,'shiftSpeedRatio':ratio,'releasedToolKeys':['W','E','R'],'contextMenuBlocksToolKeys':True,'escapeReleases':True,'exitCases':['focus','popup','dpi','outside'],'nativeCaptureRestored':True,'axisViewsPicked':orientations,'physicalKeyboardMouse':'not performed'}
    (output/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8');print(json.dumps(summary))
    return output

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();run(args.executable.resolve(),args.output.resolve())
