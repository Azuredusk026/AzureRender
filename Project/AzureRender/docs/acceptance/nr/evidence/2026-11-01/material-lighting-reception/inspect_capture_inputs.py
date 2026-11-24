import json,sys
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'));from native_render.renderdoc_capture import verify_receipt
out=run/'rdc-parallel-frame4-layer';r=json.loads((out/'receipt.json').read_text());verify_receipt(r,'capture',root/'build/ninja-msvc-release/AzurePlayer.exe',out);assert r['capture_frame']==4 and 'Vulkan' in r['api'];print('Full capture verified',r['capture']['bytes'],r['capture']['sha256'])
build=json.loads((run/'release-build.json').read_text());print('Temporal shader files',[name for name in build['shaders'] if 'temporal' in name])
d=json.loads((run/'repeat-delta-diagnostics.json').read_text());pixels=next(c for c in d['cases'] if c['frame']==3)['pixels'];point=max(pixels,key=lambda p:max(abs(a-b) for a,b in zip(p['before'],p['after'])));(run/'capture-pixel-target.json').write_bytes((json.dumps(point,indent=2)+'\n').encode());print('Selected diagnostic pixel',point)
