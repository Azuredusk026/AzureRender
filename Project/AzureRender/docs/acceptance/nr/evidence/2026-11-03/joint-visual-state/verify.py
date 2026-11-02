"""Verify actual framing, fixed-input controls and retained source provenance."""
import base64,copy,gzip,hashlib,json,subprocess,sys
from pathlib import Path
folder=Path(__file__).resolve().parent;root=next(p for p in folder.parents if (p/'CMakeLists.txt').is_file() and (p/'src').is_dir())
manifest=json.loads((folder/'manifest.json').read_text(encoding='utf8'));sha=lambda b:hashlib.sha256(b).hexdigest();entries={}
for bundle in manifest['bundles']:
    data=(folder/bundle['path']).read_bytes();assert sha(data)==bundle['sha256'];rows=json.loads(gzip.decompress(data))['files'];assert len(rows)==bundle['sourceFiles']
    for row in rows:
        raw=row['data'].encode('utf8') if row['encoding']=='utf8' else base64.b64decode(row['data']);assert len(raw)==row['bytes'] and sha(raw)==row['sha256'],row['path'];entries[row['path']]=raw
for row in manifest['directFiles']:
    data=(folder/row['path']).read_bytes();assert len(data)==row['bytes'] and sha(data)==row['sha256']
read=lambda n:json.loads(entries[n]);framing=read('framing-verification.json');assert framing['modelScale']==[1,1,1] and framing['headHeightRatio']==2
source=read('project/scene/full-world-linear.azurelevel');candidate=read('project/scene/full-world-foreground.azurelevel');oldHero=next(n for n in source['nodes'] if n['id']=='hero');hero=next(n for n in candidate['nodes'] if n['id']=='hero');hero['components']['azure.transform']['data']['translation']=oldHero['components']['azure.transform']['data']['translation'];assert candidate==source
proof=read('hair-pixel-causality.json');assert proof['nativePrimitive']==proof['currentPrimitive']==8619 and proof['nativeBase']==proof['currentBase'] and proof['currentCharacterVisibility']==1
models=read('static-model-inputs.json');assert models['one']['binarySha256']==models['object']['binarySha256'];model=models['object']['document'];material=next(m for m in model['materials'] if m['name']=='M_actor_laevat_hair_01');assert material['extras']['azureRenderMaterial']['hair']['projectedRamp']['lighting'].pop('characterVisibility')=='object-shadow';assert model==models['one']['document']
comparison=read('static-shadow-comparison.json');full=next(r for r in comparison['regions'] if r['name']=='full');assert full['pixelsOverOneByte']==73 and full['maximumByteError']==6 and not comparison['fullRegionPassed']
reports=[read(n) for n in ['visibility-captures.json','temporal-capture.json','foreground-capture.json','current-hair-capture.json','static-shadow-control.json']];assert {r['programSha256'] for r in reports}=={manifest['programSha256']}
assert sum(len(r['images']) for r in reports[0]['captures'])+len(reports[1]['images'])+len(reports[2]['images'])+len(reports[3]['images'])+1==manifest['capturedFrames']==53
assert reports[3]['sameStaticPixels']
receipt=manifest['compiledSourceReceipt'];path=root/receipt['path'];assert sha(path.read_bytes())==receipt['sha256'];subprocess.run([sys.executable,'-X','utf8',str(path.parent/'verify.py')],cwd=root,check=True)
assert manifest['visualAcceptance']=='failed-user-review'
print('Verified',len(entries),'archived files, 53 captured frames and',len(manifest['directFiles']),'images; foreground framing qualified; joint visual acceptance remains open.')
