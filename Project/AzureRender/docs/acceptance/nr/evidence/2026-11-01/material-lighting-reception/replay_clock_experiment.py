import gzip,hashlib,json,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-120125-character-environment-reception-fa70a7');build=json.loads((run/'clock-experiment-build.json').read_text());capture=json.loads((run/'rdc-parallel-frame9-layer/receipt.json').read_text());post=json.loads((run/'recording-post-pair9.json').read_text());end=post['captures'][0]['passes'][-1]['event'];out=run/'clock-replay';out.mkdir(exist_ok=False);cap=rd.OpenCaptureFile();controller=None;original=None;created=[];report=dict(status='running',stage='open',variants=[])
def progress():(run/'clock-replay.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
def output(mode):
 controller.SetFrameEvent(end,True);pipe=controller.GetPipelineState();rid=pipe.GetOutputTargets()[0].resource;data=bytes(controller.GetTextureData(rid,rd.Subresource()));p=out/(mode+'.bin.gz');p.write_bytes(gzip.compress(data,mtime=0));report['variants'].append(dict(mode=mode,bytes=len(data),path=p.relative_to(run).as_posix(),sha256=hashlib.sha256(data).hexdigest(),pixel911x4=list(controller.PickPixel(rid,911,4,rd.Subresource(),rd.CompType.Typeless).floatValue)));progress()
progress()
try:
 assert cap.OpenFile(capture['capture']['path'],'',None)==rd.ResultCode.Succeeded
 status,controller=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded,str(status)
 controller.SetFrameEvent(26483,True);pipe=controller.GetPipelineState();original=pipe.GetShader(rd.ShaderStage.Pixel);reflection=pipe.GetShaderReflection(rd.ShaderStage.Pixel);assert hashlib.sha256(bytes(reflection.rawBytes)).hexdigest()==build['variants'][0]['sha256']
 report['stage']='captured-control';progress();output('captured-control')
 for row in build['variants']:
  mode=row['mode'];report['stage']='build-'+mode;progress();data=(run/row['shader']).read_bytes();assert hashlib.sha256(data).hexdigest()==row['sha256']
  shader,errors=controller.BuildTargetShader('main',rd.ShaderEncoding.SPIRV,data,rd.ShaderCompileFlags(),rd.ShaderStage.Pixel);assert shader!=rd.ResourceId.Null(),str(errors);created.append(shader)
  report['stage']='replace-'+mode;progress();controller.ReplaceResource(original,shader);output(mode);controller.RemoveReplacement(original)
 assert report['variants'][0]['sha256']==report['variants'][1]['sha256'],'Byte-identical shader replacement must preserve the captured control'
 report.update(status='measured-clock-controls',stage='shutdown');progress()
except Exception:report.update(status='failed',error=traceback.format_exc());progress()
finally:
 if controller:
  if original:controller.RemoveReplacement(original)
  for shader in created:controller.FreeTargetResource(shader)
  controller.Shutdown()
 cap.Shutdown()
sys.exit(0)
