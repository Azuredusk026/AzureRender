"""Deliver verifiable interface source snapshots and schemas for engine designs."""
import argparse
import hashlib
import json
from pathlib import Path

def digest(data):return hashlib.sha256(data).hexdigest()
def bounded_path(root,name):
    path=(root/name).resolve()
    if Path(name).is_absolute() or not path.is_relative_to(root.resolve()):raise ValueError('Contract path escapes root: '+name)
    return path
def verify_bundle(output):
    output=output.resolve();manifest=json.loads((output/'manifest.json').read_text(encoding='utf-8'))
    if manifest.get('schemaVersion')!=1:raise ValueError('Unsupported contract snapshot version')
    expected=set(manifest['files'])|{'manifest.json'}
    actual={p.relative_to(output).as_posix() for p in output.rglob('*') if p.is_file()}
    if actual!=expected:raise ValueError('Contract snapshot inventory differs')
    for name,record in manifest['files'].items():
        path=bounded_path(output,name);data=path.read_bytes()
        if len(data)!=record['bytes'] or digest(data)!=record['sha256']:raise ValueError('Contract snapshot hash differs: '+name)
    return manifest
def write_bundle(source,manifest,output):
    source=source.resolve();output=output.resolve();names=set();items=[]
    for item in manifest['items']:
        implementation=item['implementation'];names.update(implementation['interfaceFiles'])
        items.append({'id':item['id'],'interfaces':implementation['interfaces'],
            'interfaceFiles':implementation['interfaceFiles'],'scope':implementation['scope'],
            'adoption':implementation['verification']['adoption']})
    names.update(p.relative_to(source).as_posix() for p in (source/'schemas').rglob('*.json'))
    data={}
    for name in sorted(names):
        path=bounded_path(source,name)
        if not path.is_file():raise ValueError('Missing contract input: '+name)
        data[name]=path.read_bytes()
    previous=set()
    if (output/'manifest.json').is_file():previous=set(verify_bundle(output)['files'])|{'manifest.json'}
    existing={p.relative_to(output).as_posix() for p in output.rglob('*') if p.is_file()}
    if existing-previous:raise ValueError('Output contains files not owned by the contract snapshot')
    result={'schemaVersion':1,'kind':'interface-source-snapshots-and-schemas','coveredItems':len(items),
            'referenceCommit':manifest['referenceCommit'],'items':items,
            'files':{name:{'bytes':len(value),'sha256':digest(value)} for name,value in data.items()}}
    output.mkdir(parents=True,exist_ok=True)
    for name,value in data.items():
        path=bounded_path(output,name);path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(value)
    for name in previous-set(data)-{'manifest.json'}:bounded_path(output,name).unlink()
    temporary=output/'manifest.json.tmp';temporary.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    temporary.replace(output/'manifest.json');return verify_bundle(output)
def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--manifest',type=Path,default=Path('docs/plans/engine-evolution-manifest.json'))
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--verify',action='store_true');args=parser.parse_args()
    result=verify_bundle(args.output) if args.verify else write_bundle(args.source,json.loads(args.manifest.read_text(encoding='utf-8')),args.output)
    print(json.dumps({'status':'passed','coveredItems':result['coveredItems'],'files':len(result['files'])}))
if __name__=='__main__':main()
