"""Fetch locked public glTF demos, preserving source licensing and hashes."""
import argparse, hashlib, json, urllib.request, urllib.parse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
MODELS = ('DamagedHelmet', 'ABeautifulGame', 'TransmissionRoughnessTest')
REPO = 'KhronosGroup/glTF-Sample-Assets'
LOCK = Path(__file__).resolve().parents[1] / 'assets_public/pathtracing-sources.json'
def request(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent':'AzureRender-public-demos'}), timeout=90) as response:
        return response.read()
def digest(data): return hashlib.sha256(data).hexdigest()
def fetch(output, refresh=False):
    output=Path(output); output.mkdir(parents=True,exist_ok=True)
    if refresh:
        commit=json.loads(request(f'https://api.github.com/repos/{REPO}/commits/main'))['sha']
        tree=json.loads(request(f'https://api.github.com/repos/{REPO}/git/trees/{commit}?recursive=1'))
        assert not tree.get('truncated'), 'Incomplete repository inventory'
        entries=[]
        for item in tree['tree']:
            parts=item['path'].split('/')
            if item['type']=='blob' and len(parts)>=3 and parts[0]=='Models' and parts[1] in MODELS:
                if parts[2] in ('README.md','LICENSE.md','metadata.json','glTF'):
                    entries.append({'path':item['path'],'bytes':item['size']})
        lock={'schemaVersion':1,'repository':REPO,'commit':commit,'models':list(MODELS),'files':entries}
    else: lock=json.loads(LOCK.read_text(encoding='utf-8'))
    def download(item):
        path=output/item['path'];path.parent.mkdir(parents=True,exist_ok=True)
        data=path.read_bytes() if path.exists() else b''
        if len(data)!=item['bytes'] or (item.get('sha256') and digest(data)!=item['sha256']):
            url=f"https://raw.githubusercontent.com/{REPO}/{lock['commit']}/"+urllib.parse.quote(item['path'])
            data=request(url)
            if len(data)!=item['bytes']: raise ValueError('Length mismatch: '+item['path'])
            if item.get('sha256') and digest(data)!=item['sha256']: raise ValueError('Hash mismatch: '+item['path'])
            path.write_bytes(data)
        item['sha256']=digest(data)
        return item
    with ThreadPoolExecutor(max_workers=4) as pool: lock['files']=list(pool.map(download,lock['files']))
    if lock.get('licenseDocument'):
        item=lock['licenseDocument'];path=output/item['path'];path.parent.mkdir(parents=True,exist_ok=True)
        data=path.read_bytes() if path.exists() else b''
        if digest(data)!=item['sha256']:data=request(item['url'])
        if digest(data)!=item['sha256']:raise ValueError('License hash mismatch')
        path.write_bytes(data)
    LOCK.parent.mkdir(parents=True,exist_ok=True)
    if refresh: LOCK.write_text(json.dumps(lock,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (output/'SOURCES.json').write_text(json.dumps(lock,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    for model in MODELS:
        gltf=next((output/'Models'/model/'glTF').glob('*.gltf'))
        d=json.loads(gltf.read_text(encoding='utf-8'))
        print(model, 'materials',len(d.get('materials',[])), 'extensions',d.get('extensionsUsed',[]),flush=True)
    print('Locked files:',len(lock['files']),'bytes:',sum(x['bytes'] for x in lock['files']),flush=True)
if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--refresh-lock',action='store_true')
    args=parser.parse_args();fetch(args.output,args.refresh_lock)
