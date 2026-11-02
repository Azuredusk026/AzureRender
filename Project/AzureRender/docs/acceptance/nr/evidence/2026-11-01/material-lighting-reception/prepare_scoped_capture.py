import json,sys
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'));from native_render.renderdoc_capture import verify_receipt
out=run/'rdc-layer-smoke';receipt=json.loads((out/'receipt.json').read_text());verify_receipt(receipt,'capture',root/'build/ninja-msvc-release/AzurePlayer.exe',out);assert receipt['capture_frame']==4 and 'Vulkan' in receipt['api'];print('Verified scoped capture',receipt['api'],receipt['capture']['bytes'])
text=(run/'capture_layer_smoke.py').read_text().replace("out=run/'rdc-layer-smoke'","out=run/'rdc-parallel-frame4-layer'").replace("json.loads((run/'public-gpu/release/summary.json').read_text())['cases'][0]['command'][2]","str(run/'candidate-project/full-world-hidden.azureproject')").replace("'128','--height','128','--capture-frames','30'","'2560','--height','1440','--capture-frames','12'")
assert "'--width','2560'" in text and "'--capture-frames','12'" in text;(run/'capture_parallel_scoped_rdc.py').write_bytes(text.encode())
