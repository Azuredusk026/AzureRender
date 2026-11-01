import hashlib,json,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-143507-native-cloth-variants-d877ef');root=run.parents[2];sys.path.insert(0,str(root/'tools/native_render'));from renderdoc_replay import _variable
selection=json.loads((run/'variant-pixel-selection.json').read_text());source=root/'build/runs/20261022-201845-endfield-capture-954719/Endfield-games-child_frame10401.rdc';cap=rd.OpenCaptureFile();controller=None;report=dict(status='running',traces=[])
def progress(): (run/'variant-traces.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
progress()
try:
 assert cap.OpenFile(str(source),'',None)==rd.ResultCode.Succeeded
 status,controller=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded,str(status)
 for draw in selection['draws']:
  event=draw['event'];point=draw['points'][1];row=dict(event=event,pixel=point,status='starting');report['traces'].append(row);progress()
  controller.SetFrameEvent(event,True);pipe=controller.GetPipelineState();target=pipe.GetOutputTargets()[0].resource
  history=controller.PixelHistory(target,point['x'],point['nativeY'],rd.Subresource(),rd.CompType.Typeless)
  passed=[m for m in history if m.eventId==event and not any(getattr(m,f,False) for f in ['depthTestFailed','shaderDiscarded','backfaceCulled','stencilTestFailed','scissorClipped','sampleMasked'])];assert passed
  primitive=int(passed[-1].primitiveID);inputs=rd.DebugPixelInputs();inputs.primitive=primitive;trace=None
  payload=dict(event=event,pixel=point,primitive=primitive,interpretation='RenderDoc debugger simulation; hardware output remains an independent control.',states=[],complete=False,hardwareShaderOutput=list(passed[-1].shaderOut.col.floatValue),hardwarePostBlend=list(passed[-1].postMod.col.floatValue))
  try:
   trace=controller.DebugPixel(point['x'],point['nativeY'],inputs);assert trace and trace.debugger
   for name in ['inputs','constantBlocks','readOnlyResources','readWriteResources','samplers']:payload[name]=[_variable(v) for v in getattr(trace,name)]
   while True:
    states=controller.ContinueDebug(trace.debugger)
    if not states:payload['complete']=True;break
    for state in states:
     payload['states'].append(dict(step=int(state.stepIndex),next_instruction=int(state.nextInstruction),flags=str(state.flags),changes=[dict(before=_variable(c.before),after=_variable(c.after)) for c in state.changes]));assert len(payload['states'])<20000
  finally:
   if trace:controller.FreeTrace(trace)
  path=run/('variant-trace-%d.json'%event);path.write_bytes((json.dumps(payload,separators=(',',':'))+'\n').encode());row.update(status='exported',path=path.name,steps=len(payload['states']),sha256=hashlib.sha256(path.read_bytes()).hexdigest());progress()
 report['status']='exported-variant-traces';progress()
except Exception:report.update(status='failed',error=traceback.format_exc());progress()
finally:
 if controller:controller.Shutdown()
 cap.Shutdown()
sys.exit(0)
