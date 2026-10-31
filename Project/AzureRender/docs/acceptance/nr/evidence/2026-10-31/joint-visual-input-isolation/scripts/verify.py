import hashlib,json,re,struct,subprocess,sys,types
from pathlib import Path
from PIL import Image
import numpy as np
root=Path.cwd();run=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'))
from endfield_native import npr_lighting
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
reports=[json.loads((run/f).read_text()) for f in ['summary.json','native-summary.json','context-summary.json']]
cases=[c for report in reports for c in report['cases']]
assert len(cases)==6 and all(c['exitCode']==0 for c in cases)
assert len({report['programSha256'] for report in reports})==1
base=json.loads((run/'control/frames/capture_manifest.json').read_text())
base_animations=json.loads((run/'control/runtime.json').read_text())['animations']
def gpu_value(v):
    if type(v)==float:return struct.unpack('<f',struct.pack('<f',v))[0]
    if isinstance(v,list):return [gpu_value(x) for x in v]
    return v
for case in cases:
    folder=run/case['name'];m=json.loads((folder/'frames/capture_manifest.json').read_text())
    assert m['capturedFrames']==1 and m['qaIsolation']=='beauty' and m['antiAliasing']==0
    for key in ['qaCameraPosition','qaCameraTarget','animationIndex','objectShadowPages']:
        assert m[key]==base[key],(case['name'],key)
    assert 'Allocator after unload: buffers=0 images=0' in (folder/'player.log').read_text()
    rt=json.loads((folder/'runtime.json').read_text());assert rt['scriptErrors']==[] and rt['presentationErrors']==[]
    assert rt['animations']==base_animations,(case['name'],'hero-runtime-animation')
    case['imageSha256']=sha(folder/'frames/frame_000000.png')
native=json.loads((run/'native-contexts.json').read_text())
effective=json.loads((run/'native-scene-near/frames/capture_manifest.json').read_text())['nprLighting']
assert all(gpu_value(effective[k])==gpu_value(v) for k,v in native['scene']['settings'].items())
# Preserve the existing object-light contract under both captured contexts.
old=types.ModuleType('old_decoder')
old_source=subprocess.run(['git','show','HEAD:Project/AzureRender/tools/endfield_native/npr_lighting.py'],capture_output=True,check=True,text=True).stdout
exec(compile(old_source,'old_decoder','exec'),old.__dict__)
compat=[]
for name,ctx in [(k,native[k]) for k in ['scene','portrait']]:
    data=Path(ctx['source']).read_bytes();assert sha(Path(ctx['source']))==ctx['sourceSha256']
    rows={i:list(struct.unpack_from('<4f',data,i*16)) for i in range(len(data)//16)}
    for ambient,main in [(112,115),(113,114)]:
        objects={0:[.2,-.9,.3,0],3:[.5,.7,.9,1.6]}
        a=old.decode_lighting(rows,objects,ambient_row=ambient,main_color_row=main)
        b=npr_lighting.decode_lighting(rows,objects,ambient_row=ambient,main_color_row=main)
        assert a==b
        compat.append({'context':name,'ambientRow':ambient,'mainColorRow':main,'resultExact':True})
assert (run/'control/frames/frame_000000.png').read_bytes()==(root/'build/runs/20261031-172112-hair-source-inputs-14d199/beauty/frames/frame_000000.png').read_bytes()
regions={'hair-crown':[710,400,820,550],'face':[705,852,795,885],'eye-left':[625,784,683,814]}
stats=[]
baseline=np.asarray(Image.open(run/'control/frames/frame_000000.png'))[:,:,:3].astype(int)
for case in cases:
    pixels=np.asarray(Image.open(run/case['name']/'frames/frame_000000.png'))[:,:,:3].astype(int)
    for name,(x0,y0,x1,y1) in regions.items():
        v=pixels[y0:y1,x0:x1];d=np.abs(v-baseline[y0:y1,x0:x1])
        stats.append(dict(case=case['name'],region=name,meanRgb=v.mean((0,1)).tolist(),meanAbsoluteByteDifference=float(d.mean())))
result={'status':'diagnostic-input-verification-passed','visualAcceptance':'failed-user-review',
    'scope':'Six native Player diagnostic captures and source-context decoder checks; complete visual quality is not accepted.',
    'programSha256':reports[0]['programSha256'],'captures':[{k:c[k] for k in ['name','exitCode','seconds','imageSha256']} for c in cases],
    'sourceContextGpuSettingsMatched':True,'previousObjectDecoderComparisons':compat,'regionStatistics':stats}
(run/'verification.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k not in ['regionStatistics','captures']},indent=2))
