import json
from pathlib import Path
from PIL import Image
run=Path(__file__).resolve().parent;d=json.loads((run/'parallel-pixel-history.json').read_text());names={v:k for k,v in json.loads((run/'release-build.json').read_text())['shaders'].items()};print(d['status'],d.get('target'),len(d['rows']),d.get('error',''))
for row in d['rows']:
 print(row['event'],row['primitive'],names.get(row.get('pixelShaderSha256')),row['pre'],row['post'],'discard',row['shaderDiscarded'],'depthfail',row['depthTestFailed'])
for folder in ['native-captures/full-world-hidden','compatibility-repeat/full-world-hidden','determinism-captures/serial-a','rdc-parallel-frame4-layer/target']:
 p=run/folder/'frames/frame_000003.png';im=Image.open(p);print('PNG',folder,im.getpixel((927,3)))
p=run/'candidate-project/scene/full-world-hidden.azurelevel';s=json.loads(p.read_text())['renderSettings'];print('Scene antiAliasing',s['antiAliasing'])
