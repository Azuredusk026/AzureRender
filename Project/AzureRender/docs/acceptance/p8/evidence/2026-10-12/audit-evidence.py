from pathlib import Path
import hashlib,json,subprocess,sys
root=Path.cwd();out=root/'build/p8-format-closure-2026-10-12';m=json.loads((root/'docs/plans/asset-format-manifest.json').read_text(encoding='utf-8'));rows=[];phases=[]
for phase in m['phases']:
 if not phase.get('evidence'):continue
 p=root/phase['evidence'];d=json.loads(p.read_text(encoding='utf-8'));files=d['files'];entries=[dict(path=k,sha256=v) for k,v in files.items()] if isinstance(files,dict) else files
 for row in entries:
  file=p.parent/row['path'];data=file.read_bytes();assert hashlib.sha256(data).hexdigest()==row['sha256'],file
  if 'bytes' in row:assert len(data)==row['bytes'],file
  rows.append(dict(path='Project/AzureRender/'+file.relative_to(root).as_posix(),sha256=row['sha256']))
 phases.append(dict(phase=phase['id'],files=len(entries),manifestSha256=hashlib.sha256(p.read_bytes()).hexdigest()))
result=subprocess.run(['git','cat-file','--batch'],input=''.join((':' if '--index' in sys.argv else 'HEAD:')+r['path']+'\n' for r in rows).encode('utf-8'),capture_output=True,check=True);data=result.stdout;offset=0;differences=[]
for row in rows:
 end=data.index(b'\n',offset);header=data[offset:end].split();assert header[1]==b'blob',row['path'];size=int(header[2]);offset=end+1;blob=data[offset:offset+size];
 if hashlib.sha256(blob).hexdigest()!=row['sha256']:differences.append(dict(row,gitSha256=hashlib.sha256(blob).hexdigest()))
 offset+=size+1
assert offset==len(data)
summary=dict(passed=not differences,gitByteDifferences=differences,phaseCount=len(phases),rawCommittedFiles=len(rows),workingTreeBytes=True,gitBlobBytes=not differences,phases=phases);(out/'evidence-audit.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8');print(json.dumps(summary))
