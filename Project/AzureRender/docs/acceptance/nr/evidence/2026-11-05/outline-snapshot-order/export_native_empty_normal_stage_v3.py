from pathlib import Path
import renderdoc as rd,json,sys,traceback
r=Path('D:\\Projects\\FYP\\Project\\AzureRender\\build\\runs\\20261103-234012-character-reference-stages-5abe6c');out=r/'native-empty-normal-stage-v3';out.mkdir(exist_ok=True);cap=rd.OpenCaptureFile();c=None;replacements=[];report=dict(status='starting')
try:
 assert cap.OpenFile(str(r.parent/'20261022-201845-endfield-capture-954719/Endfield-games-child_frame10401.rdc'),'',None)==rd.ResultCode.Succeeded
 status,c=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded
 def walk(a):
  for v in a:
   yield v
   yield from walk(v.children)
 actions=list(walk(c.GetRootActions()));shaders={};actorEvents=[296,310,328,345,363,377,391,404,422,440,783,804,825,846,868,889,910,928,950,1018,1038,1059,1079,1099,1116,1138,1230,1245,1291,1314,1339,1358,1383]
 for event in actorEvents:
  c.SetFrameEvent(event,True);pipe=c.GetPipelineState();ref=pipe.GetShaderReflection(rd.ShaderStage.Vertex);shader=pipe.GetShader(rd.ShaderStage.Vertex)
  if str(shader) in shaders:shaders[str(shader)]['events'].append(event);continue
  signature=[];declarations=[];assignments=[]
  for i,v in enumerate(ref.outputSignature):
   sem=v.semanticName+str(v.semanticIndex);typ='uint' if 'UInt' in str(v.varType) else 'int' if 'SInt' in str(v.varType) else 'float';typ+=str(v.compCount) if v.compCount>1 else ''
   declarations.append('%s q%d : %s;'%(typ,i,sem));assignments.append('o.q%d=%s;'%(i,'float4(0,0,2,1)' if 'Position' in str(v.systemValue) else '0'));signature.append(dict(semantic=sem,type=typ,systemValue=str(v.systemValue)))
  source='struct Output {'+' '.join(declarations)+'}; Output main(){Output o;'+' '.join(assignments)+'return o;}'
  replacement,errors=c.BuildTargetShader('main',rd.ShaderEncoding.HLSL,source.encode(),rd.ShaderCompileFlags(),rd.ShaderStage.Vertex)
  assert replacement!=rd.ResourceId.Null(),errors
  shaders[str(shader)]=dict(shader=shader,replacement=replacement,events=[event],signature=signature,source=source,warnings=errors)
 c.SetFrameEvent(1383,True);target=c.GetPipelineState().GetOutputTargets()[0].resource;report['target']=str(target);(out/'original.bin').write_bytes(bytes(c.GetTextureData(target,rd.Subresource())))
 normal=next(t.resourceId for t in c.GetTextures() if str(t.resourceId)=='ResourceId::141946');(out/'original-normal.bin').write_bytes(bytes(c.GetTextureData(normal,rd.Subresource())))
 for row in shaders.values():c.ReplaceResource(row['shader'],row['replacement']);replacements.append(row)
 c.SetFrameEvent(1383,True);(out/'empty.bin').write_bytes(bytes(c.GetTextureData(target,rd.Subresource()))); (out/'empty-normal.bin').write_bytes(bytes(c.GetTextureData(normal,rd.Subresource())))
 report.update(status='exported',actorEvents=actorEvents,shaders=[{k:(str(v) if k in ['shader','replacement'] else v) for k,v in row.items()} for row in shaders.values()])
except Exception:report.update(status='failed',error=traceback.format_exc())
finally:
 if c:
  for row in replacements:c.RemoveReplacement(row['shader']);c.FreeTargetResource(row['replacement'])
  c.Shutdown()
 cap.Shutdown();(out/'summary.json').write_text(json.dumps(report,indent=2))
sys.exit(0 if report['status']=='exported' else 1)
