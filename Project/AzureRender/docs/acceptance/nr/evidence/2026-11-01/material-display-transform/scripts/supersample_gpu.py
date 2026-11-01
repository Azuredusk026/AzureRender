import argparse,json,re,subprocess,sys,hashlib
from pathlib import Path
import numpy as np
from PIL import Image
root=Path.cwd();run=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'))
from test_material_display_gpu import prepare,linear,CONSTANT
parser=argparse.ArgumentParser();parser.add_argument('--configuration',choices=['debug','release'],required=True);args=parser.parse_args()
player=root/('build/ninja-msvc-'+args.configuration)/'AzurePlayer.exe';digest=hashlib.sha256(player.read_bytes()).hexdigest();images={};rows=[]
try:
 for mode in ['reference','scene','linear']:
  folder=run/'supersample-gpu'/args.configuration/mode;definition=prepare(folder,'multisample',mode)
  path=folder/'level.azurelevel';level=json.loads(path.read_text());level['renderSettings']['antiAliasing']=2;path.write_text(json.dumps(level))
  command=[str(player),'--project',str(definition['project']),'--width','128','--height','128','--capture-frames','1','--fixed-frame-step','--capture-dir',str(folder/'frames'),'--qa-camera','scene','--qa-light','scene','--runtime-report',str(folder/'runtime.json')]
  with (folder/'player.log').open('wb') as log:result=subprocess.run(command,cwd=folder,stdout=log,stderr=log,timeout=150)
  assert result.returncode==0,(mode,result.returncode)
  log=(folder/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I),mode
  records=[json.loads(line) for line in (folder/'captures/azureplayer.log.jsonl').read_text().splitlines()]
  assert any(r['message']=='Allocator after unload: buffers=0 images=0' for r in records)
  manifest=json.loads((folder/'frames/capture_manifest.json').read_text());assert manifest['sceneRenderWidth']==256 and manifest['sceneRenderHeight']==256
  assert manifest['sceneDisplayMaskBytes']==256*256*manifest['sceneDisplayMaskImages'] and manifest['rasterizationSamples']==1
  runtime=json.loads((folder/'runtime.json').read_text());assert not runtime['scriptErrors'] and not runtime['presentationErrors']
  images[mode]=np.array(Image.open(folder/'frames/frame_000000.png').convert('RGB'));rows.append(dict(mode=mode,command=command,status='captured'))
 error=float(np.abs(linear(images['reference'][48:80,48:80])-linear(images['linear'][48:80,48:80])).max())
 assert error<=.008,error
 assert np.array_equal(images['linear'][:16,:16],images['scene'][:16,:16])
 assert np.abs(linear(images['scene'][64,64])-CONSTANT).max()<=.008
 assert hashlib.sha256(player.read_bytes()).hexdigest()==digest
 report=dict(status='passed',programSha256=digest,cases=rows,maximumInteriorLinearError=error,scope='Supersampled interior display and background ownership; partial-coverage edges have separate temporal and multisample gates.')
 (run/('supersample-gpu-'+args.configuration+'-summary.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(status='passed',maximumInteriorLinearError=error)))
except Exception as error:
 (run/('supersample-gpu-'+args.configuration+'-failed.json')).write_text(json.dumps(dict(status='failed',failure=str(error),cases=rows),indent=2)+'\n');raise
