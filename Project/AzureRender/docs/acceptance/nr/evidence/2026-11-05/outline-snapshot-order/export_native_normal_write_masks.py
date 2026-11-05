from pathlib import Path
import renderdoc as rd,json,sys,traceback
r=Path('D:\\Projects\\FYP\\Project\\AzureRender\\build\\runs\\20261103-234012-character-reference-stages-5abe6c');cap=rd.OpenCaptureFile();c=None;rows=[]
try:
 assert cap.OpenFile(str(r.parent/'20261022-201845-endfield-capture-954719/Endfield-games-child_frame10401.rdc'),'',None)==rd.ResultCode.Succeeded
 result,c=cap.OpenCapture(rd.ReplayOptions(),None);assert result==rd.ResultCode.Succeeded
 for e in [422,440,460,950,1138]:
  c.SetFrameEvent(e,True);p=c.GetD3D11PipelineState();b=p.outputMerger.blendState;raster=p.rasterizer.state
  row=dict(event=e,outputs=[str(t.resource) for t in c.GetPipelineState().GetOutputTargets()],blendFields=[n for n in dir(b) if not n.startswith('_')],targets=[{n:str(getattr(t,n)) for n in dir(t) if n not in ['this','thisown'] and not n.startswith('_') and not callable(getattr(t,n))} for t in b.blends],cull=str(raster.cullMode),frontCCW=raster.frontCCW);rows.append(row)
except Exception:rows.append(dict(error=traceback.format_exc()))
finally:
 if c:c.Shutdown()
 cap.Shutdown();(r/'native-normal-stage-write-masks.json').write_text(json.dumps(rows,indent=2))
sys.exit(0)
