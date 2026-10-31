import hashlib,json,subprocess
from pathlib import Path
import numpy as np
from PIL import Image
root=Path.cwd();run=Path(__file__).resolve().parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
frozen=json.loads((run/'frozen-build.json').read_text())
for name,receipt in frozen['sourceFiles'].items():assert sha(root/name)==receipt['workingSha256'],name
remaining=json.loads((run/'remaining-status.json').read_text());assert remaining['status']=='passed'
for cfg in ['release','debug']:
    player=root/f'build/ninja-msvc-{cfg}/AzurePlayer.exe';assert sha(player)==frozen['programs'][cfg]['playerSha256']
    result=subprocess.run([str(root/f'build/ninja-msvc-{cfg}/AzureRenderSceneGraphTests.exe')],capture_output=True,timeout=30)
    assert result.returncode==0,(cfg,result.stdout,result.stderr)
    final=json.loads((run/f'gpu-final-{cfg}/summary.json').read_text());assert final['status']=='passed' and len(final['captures'])==7
    assert final['playerSha256']==sha(player)
    assert all(c['validationEnabled']==(cfg=='debug') for c in final['captures'])
    for kind,expected in [('hair',9),('eye',6),('object',7)]:
        report=json.loads((run/f'{kind}-{cfg}/summary.json').read_text());assert report['status']=='passed'
        assert report['playerSha256']==sha(player)
        captures=report.get('captures',report.get('cases',[]));assert len(captures)==expected,(kind,cfg,len(captures))
release=json.loads((run/'gpu-final-release-all/summary.json').read_text())
assert release['status']=='passed' and len(release['captures'])==54
near=run/'character-capture/near-control/frames/frame_000000.png'
old=root/'build/runs/20261031-222119-combined-visual-isolation-ad6878/control/frames/frame_000000.png'
assert near.read_bytes()==old.read_bytes()
comparisons=[]
pairs=[('full-control-v8',run/'character-capture/full-control/frames',root/'build/runs/20261031-172112-hair-source-inputs-14d199/regression/full-restored/frames'),
    ('hidden-environment',run/'character-capture/hidden-control/frames',run/'character-capture/hidden-reference-light/frames')]
for name,left,right in pairs:
    for index in range(12):
        filename=f'frame_{index:06d}.png';a=left/filename;b=right/filename
        x=np.asarray(Image.open(a)).astype(int);y=np.asarray(Image.open(b)).astype(int);delta=np.abs(x-y)
        comparisons.append({'pair':name,'frame':index,'pngBytesExact':a.read_bytes()==b.read_bytes(),
            'differentPixels':int(np.any(delta,axis=2).sum()),'maximumByteError':int(delta.max()),'imageSha256':sha(a),'referenceSha256':sha(b)})
report={'status':'checks-recorded','visualAcceptance':'failed-user-review',
    'scope':'Receiver-aware light selection, runtime resource ownership, baseline compatibility and diagnostic reference lighting; complete actor/scene quality remains unaccepted.',
    'programs':frozen['programs'],'releaseNprCases':54,'finalReceiverCasesPerConfiguration':7,
    'finalRelatedCasesPerConfiguration':22,'releaseNprMaximumLinearError':max(c['maximumLinearError'] for c in release['captures']),
    'defaultNearPngExact':True,'comparisons':comparisons,
    'fullControlExactFrames':sum(x['pngBytesExact'] for x in comparisons if x['pair']=='full-control-v8'),
    'hiddenEnvironmentExactFrames':sum(x['pngBytesExact'] for x in comparisons if x['pair']=='hidden-environment')}
(run/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ['comparisons','programs']},indent=2))
assert report['fullControlExactFrames']==12,'Default full view compatibility remains failed'
assert report['hiddenEnvironmentExactFrames']==12,'Hidden environment compatibility remains failed'
visible=run/'character-capture/full-reference-light'
capture=json.loads((visible/'frames/capture_manifest.json').read_text())
assert capture['capturedFrames']==12 and capture['objectShadowPages']==1
rt=json.loads((visible/'runtime.json').read_text());assert rt['scriptErrors']==[] and rt['presentationErrors']==[]
text=(visible/'player.log').read_text();assert 'Clustered lighting: 16 lights' in text and 'Allocator after unload: buffers=0 images=0' in text
report['visibleReferenceFullCapturedFrames']=12
(run/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
