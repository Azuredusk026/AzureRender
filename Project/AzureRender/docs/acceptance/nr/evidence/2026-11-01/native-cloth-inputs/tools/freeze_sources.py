import gzip,hashlib,json,re
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();legacy=root/'build/runs/20261023-140322-skin-ramp-capture-validation-4e1751';cloth=root/'build/runs/20261026-062121-cloth-specular-ramp-capture-0e33fc';active=root/'build/runs/20261101-120125-character-environment-reception-fa70a7';source=[]
for path in [cloth/'e868-ps.asm',*[legacy/f'e{event}-ps-complete.asm' for event in [889,910,928]],*[legacy/f'e{event}-pipeline.json' for event in [889,910,928]],active/'clothing-source-brief.json',active/'lighting-candidate.json']:
    assert path.is_file();data=path.read_bytes();name=path.name;(run/'source').mkdir(exist_ok=True);(run/'source'/(name+'.gz')).write_bytes(gzip.compress(data,mtime=0));row=dict(path=path.relative_to(root).as_posix(),sha256=sha(path),bytes=len(data),frozen='source/'+name+'.gz');source.append(row)
shader_rows=[]
for event,path in [(868,cloth/'e868-ps.asm'),*[(e,legacy/f'e{e}-ps-complete.asm') for e in [889,910,928]]]:
    text=path.read_text();instructions=re.findall(r'^\s*(\d+):\s*(.*)$',text,re.M);shader_rows.append(dict(event=event,sha256=sha(path),lines=len(text.splitlines()),instructions=len(instructions),constantBufferRows=re.findall(r'dcl_constantbuffer (cb\d+\[\d+\])',text)))
assert sha(legacy/'e910-ps-complete.asm')==sha(legacy/'e928-ps-complete.asm'),'The presumed shared clothing shader must be checked before any reuse'
report=dict(status='frozen-source-inputs',materialIndicesByEvent={'868':4,'889':1,'910':5,'928':2},sharedPixelShaderEvents=[910,928],files=source,shaders=shader_rows,visualAcceptance='failed-user-review')
(run/'source-manifest.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps({k:v for k,v in report.items() if k!='files'}))
