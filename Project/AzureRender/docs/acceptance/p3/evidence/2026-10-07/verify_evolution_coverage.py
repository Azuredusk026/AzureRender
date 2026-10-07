"""Verify adopted engine designs against interfaces, tests and committed evidence."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

EXPECTED={*(f'A{i}' for i in range(1,7)),*(f'B{i}' for i in range(1,9)),*(f'C{i}' for i in range(1,5))}
TEXT_SUFFIXES={'.cpp','.hpp','.h','.py','.cs','.csproj','.json','.txt','.cmake','.md','.vert','.frag','.comp','.slang','.glsl'}
def require(condition,message):
    if not condition:raise ValueError(message)
def safe_path(root,name):
    require(isinstance(name,str) and bool(name),'Invalid path')
    path=(root/name).resolve()
    require(not Path(name).is_absolute() and path.is_relative_to(root.resolve()),'Evidence path escapes root: '+name)
    require(path.is_file(),'Missing path: '+str(path))
    return path
def git(root,*args):
    result=subprocess.run(['git','-C',str(root),*args],capture_output=True)
    require(result.returncode==0,'Git commit or historical input cannot be resolved: '+' '.join(args))
    return result.stdout
def matches_source(data,expected,name,form=None,commit=None):
    candidates=[data]
    if Path(name).suffix in TEXT_SUFFIXES:
        lf=data.replace(b'\r\n',b'\n');candidates.extend([lf,lf.replace(b'\n',b'\r\n')])
        if form:
            lines=lf.splitlines(keepends=True)
            if (form.get('gitCommit')!=commit or form.get('rawSha256')!=expected
                    or form.get('normalizedSha256')!=hashlib.sha256(lf).hexdigest()
                    or form.get('lineCount')!=len(lines)):return False
            bits=[False]*len(lines);previous=0
            for span in form.get('crlfLineRanges',[]):
                if (not isinstance(span,list) or len(span)!=2 or any(type(x)!=int for x in span)
                        or not previous<=span[0]<span[1]<=len(lines)):return False
                start,end=span;bits[start:end]=[True]*(end-start);previous=end
            candidates.append(b''.join(line.replace(b'\n',b'\r\n') if bit else line for line,bit in zip(lines,bits)))
    return any(hashlib.sha256(value).hexdigest()==expected for value in candidates)
def verify(source:Path,manifest:dict,discovery:dict)->dict:
    source=source.resolve();repository=Path(git(source,'rev-parse','--show-toplevel').decode('utf-8').strip())
    require(manifest.get('schemaVersion')==1,'Unsupported coverage manifest version')
    items=manifest.get('items',[]);ids=[i.get('id') for i in items]
    require(len(ids)==18 and set(ids)==EXPECTED,'Exactly 18 unique A/B/C items are required')
    phases={p['id']:p for p in manifest['phases']};tasks={t['id'] for t in manifest['tasks']}
    tests={t['name'] for t in discovery['tests']}
    history={}
    for line in git(source,'log','--format=%H%x00%s').decode('utf-8').splitlines():
        commit,title=line.split('\x00',1);history.setdefault(title,commit)
    outcomes=set(manifest['adoptionOutcomes']);reports=[];phase_reports={}
    def phase_evidence(phase_id):
        if phase_id in phase_reports:return phase_reports[phase_id]
        require(phase_id in phases,'Unknown phase: '+phase_id)
        phase=phases[phase_id];v=phase.get('verification',{})
        commit=v.get('commit') or history.get(v.get('commitTitle',''))
        require(bool(commit),'Missing phase commit: '+phase_id)
        commit=git(source,'rev-parse','--verify',commit+'^{commit}').decode().strip()
        safe_path(source,v['acceptance']);path=safe_path(source,v['evidence'])
        evidence=json.loads(path.read_text(encoding='utf-8-sig'))
        require(evidence.get('status')=='passed','Evidence status is not passed: '+phase_id)
        require(evidence.get('phase')==phase_id,'Evidence phase mismatch: '+phase_id)
        require(evidence.get('referenceCommit')==manifest['referenceCommit'],'Reference commit mismatch: '+phase_id)
        require(evidence.get('parentCommit')==git(source,'rev-parse',commit+'^').decode().strip(),'Phase parent commit mismatch: '+phase_id)
        hashes=evidence.get('files',{});require(bool(hashes),'Empty evidence files: '+phase_id)
        passed_counts=set()
        for name,expected in hashes.items():
            archived=safe_path(path.parent,name);data=archived.read_bytes()
            require(hashlib.sha256(data).hexdigest()==expected,'Archived evidence hash mismatch: '+name)
            if archived.suffix=='.log':
                passed_counts.update(int(n) for n in re.findall(r'100% tests passed, 0 tests failed out of (\d+)',data.decode('utf-8',errors='replace')))
        for configuration in ('Debug','Release'):
            count=evidence.get('tests',{}).get(configuration,0)
            require(count>0 and count in passed_counts,'Missing complete test evidence: '+phase_id+' '+configuration)
        inputs=evidence.get('implementationInputs',{});require(bool(inputs),'Empty historical implementation inputs: '+phase_id)
        for name,expected in inputs.items():
            require(not Path(name).is_absolute() and '..' not in Path(name).parts,'Invalid historical source path: '+name)
            data=git(repository,'show',commit+':'+name)
            form=manifest.get('historicalByteForms',{}).get(phase_id,{}).get(name)
            require(matches_source(data,expected,name,form,commit),'Historical implementation hash mismatch: '+name)
        receipt_name=path.relative_to(repository).as_posix()
        require(matches_source(git(repository,'show',commit+':'+receipt_name),hashlib.sha256(path.read_bytes()).hexdigest(),receipt_name),
                'Committed evidence receipt hash mismatch: '+phase_id)
        result={'phase':phase_id,'commit':commit,'evidence':v['evidence'],'evidenceFiles':len(hashes),
                'implementationInputs':len(inputs),'tests':evidence['tests'],'sourceItems':evidence['sourceItems']}
        phase_reports[phase_id]=result;return result
    for item in items:
        ident=item['id'];implementation=item.get('implementation')
        require(isinstance(implementation,dict),'Missing implementation: '+ident)
        require(item.get('tasks') and set(item['tasks'])<=tasks,'Invalid task mapping: '+ident)
        require(item.get('phases'),'Missing phase mapping: '+ident)
        for phase_id in item['phases']:
            phase=phase_evidence(phase_id)
            require(ident in phase['sourceItems'],'Item absent from phase evidence: '+ident)
        v=implementation.get('verification',{})
        require(v.get('adoption') in outcomes,'Missing adoption outcome: '+ident)
        require(implementation.get('scope'),'Missing adoption scope: '+ident)
        require(implementation.get('tests') and set(implementation['tests'])<=tests,'Missing discovered test: '+ident)
        risk=implementation.get('riskResolution',{})
        require(bool(risk.get('tests') or risk.get('limits')),'Missing risk resolution: '+ident)
        require(set(risk.get('tests',[]))<=tests,'Missing discovered risk test: '+ident)
        files=implementation.get('interfaceFiles',[])
        require(files and implementation.get('interfaces'),'Missing interface files: '+ident)
        text='\n'.join(safe_path(source,name).read_text(encoding='utf-8',errors='replace') for name in files)
        for symbol in implementation['interfaces']:
            require(re.search(r'\b'+re.escape(symbol)+r'\b',text) is not None,'Missing interface symbol: '+ident+' '+symbol)
        safe_path(source,v['acceptance']);safe_path(source,v['evidence'])
        require(any(v['evidence']==phases[p]['verification']['evidence'] for p in item['phases']),
                'Implementation evidence belongs to another phase: '+ident)
        reports.append({'id':ident,'interfaces':implementation['interfaces'],'interfaceFiles':files,
            'tasks':item['tasks'],'tests':implementation['tests'],'phases':item['phases'],
            'adoption':v['adoption'],'scope':implementation['scope'],'riskResolution':risk})
    return {'schemaVersion':1,'status':'passed','coveredItems':18,'referenceCommit':manifest['referenceCommit'],
            'phases':list(phase_reports.values()),'items':reports,'historicalSourcePolicy':'phase commit bytes; LF/CRLF normalization for text only'}
def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--manifest',type=Path,required=True)
    parser.add_argument('--evidence',type=Path,required=True)
    parser.add_argument('--discovery',type=Path)
    args=parser.parse_args()
    discovery=args.discovery or args.evidence/'release-test-discovery.json'
    report=verify(args.source,json.loads(args.manifest.read_text(encoding='utf-8-sig')),json.loads(discovery.read_text(encoding='utf-8-sig')))
    args.evidence.mkdir(parents=True,exist_ok=True)
    (args.evidence/'coverage.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'status':'passed','coveredItems':18,'phases':len(report['phases'])}))
if __name__=='__main__':main()
