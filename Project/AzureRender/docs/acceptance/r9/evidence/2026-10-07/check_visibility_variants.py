"""Exercise every existing winding and clip fixture through both GPU variants."""
import hashlib
import json
import os
from pathlib import Path
import sys
from PIL import Image
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools'))
import test_scene_visibility_gpu as visibility
os.environ.update(VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LAYER_VALIDATE_SYNC='1',VK_LOADER_DEBUG='error,layer')
device=sys.argv[1] if len(sys.argv)>1 else 'rtx'
work=root/'build/evolution/r9'/('visibility-'+device)
work.mkdir()
original=visibility.capture
summaries=[]
for name,flags in [('default',()),('prototype',('--visibility-prototype',)),('prototype-fixed',('--visibility-prototype','--disable-bindless'))]:
 def capture(*args,flagsForCase=flags,**kwargs):
  combined=tuple(kwargs.pop('flags',()))+flagsForCase
  return original(*args,flags=combined,**kwargs)
 visibility.capture=capture
 folder=work/name
 summary=visibility.run(root/'build/ninja-msvc-release/AzurePlayer.exe',folder)
 hashes={}
 for path in folder.glob('*-capture/frame_*.png'):
  with Image.open(path) as image:hashes[path.relative_to(folder).as_posix()]=hashlib.sha256(image.convert('RGBA').tobytes()).hexdigest()
 if summaries:assert hashes==summaries[0]['pixelHashes'],'Visibility material, mirror, boundary, depth or transparency image mismatch'
 summaries.append(dict(name=name,result=summary,pixelHashes=hashes))
 print(name+' visibility fixtures passed',flush=True)
(work/'summary.json').write_text(json.dumps(dict(status='passed',variants=summaries),indent=2),encoding='utf-8')
