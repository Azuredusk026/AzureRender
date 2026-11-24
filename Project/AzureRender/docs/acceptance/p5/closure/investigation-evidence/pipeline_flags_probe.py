"""Diagnostic-only Vulkan pipeline optimization flag contrast in an owned target."""
from pathlib import Path
import json
state=None
def callback(frame,location,internal):
    import lldb
    try:
        count=frame.FindRegister('r8').GetValueAsUnsigned()&0xffffffff
        if count!=1:raise RuntimeError('Probe accepts one create-info only')
        pointer=frame.FindRegister('r9').GetValueAsUnsigned()+16
        process=frame.GetThread().GetProcess();error=lldb.SBError()
        data=process.ReadMemory(pointer,4,error)
        if not error.Success() or len(data)!=4:raise RuntimeError('Cannot read flags')
        flags=int.from_bytes(data,'little');desired=flags|1 if state['disableOptimization'] else flags
        if desired!=flags:
            if process.WriteMemory(pointer,desired.to_bytes(4,'little'),error)!=4 or not error.Success():
                raise RuntimeError('Cannot write diagnostic pipeline flags')
        state['calls'].append(dict(api=state['apis'][location.GetBreakpoint().GetID()],old=flags,new=desired))
    except Exception as e:state['errors'].append(str(e))
    return False
def configure(debugger,path):
    global state
    state=json.loads(Path(path).read_text());state.update(calls=[],errors=[],apis={})
    for api in ('vkCreateGraphicsPipelines','vkCreateComputePipelines'):
        b=debugger.GetSelectedTarget().BreakpointCreateByName(api,'vulkan-1.dll')
        b.SetScriptCallbackFunction(__name__+'.callback');state['apis'][b.GetID()]=api
def finish(debugger):
    state['exitCode']=debugger.GetSelectedTarget().GetProcess().GetExitStatus()
    state['diagnosticOnly']=True
    Path(state['output']).write_text(json.dumps(state,indent=2)+'\n')
