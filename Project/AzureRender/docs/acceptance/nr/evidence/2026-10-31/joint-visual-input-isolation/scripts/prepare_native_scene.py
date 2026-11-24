import copy,hashlib,json,struct,sys
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'))
from endfield_native.npr_lighting import decode_scene_settings
source=root/'build/runs/20261022-212213-native-binding-restoration-d39ea8'
contexts={}
for name,relative,ambient in [('scene','frame3534/e8944-ps-cb1.bin',113),('portrait','frame10401/e804-PS-cb1.bin',113)]:
    path=source/relative;data=path.read_bytes();assert len(data)==3328
    rows={i:list(struct.unpack_from('<4f',data,i*16)) for i in range(len(data)//16)}
    contexts[name]={'source':str(path),'sourceSha256':hashlib.sha256(data).hexdigest(),
        'sourceBytes':len(data),'layoutContract':'tools/endfield_native/npr_lighting.py confirmed global layout',
        'settings':decode_scene_settings(rows,ambient_row=ambient)}
assert contexts['scene']['sourceSha256']=='c588caf1f324c256cf22285ccb1dc0317f6373219e0af467189648d550114007'
project=run/'project';control=json.loads((project/'scene/control.azurelevel').read_text())
effective=json.loads((run/'control/frames/capture_manifest.json').read_text())['nprLighting']
settings=contexts['scene']['settings'];settings['specularFloor']=control['renderSettings']['nprLighting']['specularFloor']
def gpu_value(value):
    if isinstance(value,list):return [gpu_value(x) for x in value]
    if type(value)==float:return struct.unpack('<f',struct.pack('<f',value))[0]
    return value
different={k:{'current':effective.get(k),'nativeScene':v} for k,v in settings.items() if gpu_value(effective.get(k))!=gpu_value(v)}
assert set(different)=={'directionBias','directionGain'},different
contexts['baselineComparison']={'status':'failed-source-input-match','differences':different,
    'scope':'Global composition only; object lighting, visibility, material colors, HDR and complete appearance remain separate conditions.'}
scene=copy.deepcopy(control);scene['renderSettings']['nprLighting']=settings
descriptor=json.loads((project/'control.azureproject').read_text())
cases=[]
for name,level in [('native-scene-near',scene)]:
    path=project/'scene'/(name+'.azurelevel');path.write_text(json.dumps(level,indent=2)+'\n')
    config=copy.deepcopy(descriptor);config['id']=name;config['startupScene']='stage:/'+path.name
    dest=project/(name+'.azureproject');dest.write_text(json.dumps(config,indent=2)+'\n')
    cases.append({'name':name,'project':str(dest),'levelSha256':hashlib.sha256(path.read_bytes()).hexdigest()})
# Hold geometry, post, lights, pose, material documents and shadow policy fixed.
restored=copy.deepcopy(scene);restored['renderSettings']['nprLighting']=control['renderSettings']['nprLighting']
assert restored==control
(run/'native-contexts.json').write_text(json.dumps(contexts,indent=2)+'\n')
(run/'native-cases.json').write_text(json.dumps(cases,indent=2)+'\n')
script=(run/'capture.py').read_text().replace("run/'cases.json'","run/'native-cases.json'").replace("run/'summary.json'","run/'native-summary.json'")
(run/'capture_native.py').write_text(script)
print(json.dumps(contexts['baselineComparison'],indent=2))
