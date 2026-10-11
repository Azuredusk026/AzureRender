from pathlib import Path
import concurrent.futures,hashlib,json,urllib.request
cache=Path('C:/Users/23587/Tools/vcpkg/downloads')
sources=[('xiph/opus','v1.6.1','b9a504f8576c977df57caee808412ab95e7424e9a64e3dd4045f05616c3ed58706bc3549195b61e25721e299547509d15206d246d96f1a307b82562b703b412c'),('xiph/opusfile','9d718345ce03b2fad5d7d28e0bcd1cc69ab2b166','17e323d6c031330f10b045a1438cd7ba44e2ac313ec7b3d69a8041dfa927c3f501d04246d974109fbd68dfa1a8d7b63584d96caed69ad5e38b68358aa35af65a')]
def fetch(row):
    repo,ref,expected=row;target=cache/(repo.replace('/','-')+'-'+ref+'.tar.gz');url='https://codeload.github.com/'+repo+'/tar.gz/'+ref
    if target.exists() and hashlib.sha512(target.read_bytes()).hexdigest()==expected:data=target.read_bytes()
    else:
        with urllib.request.urlopen(url,timeout=45) as response:data=response.read()
        assert hashlib.sha512(data).hexdigest()==expected
        temp=target.with_suffix('.fixedtmp');temp.write_bytes(data);temp.replace(target)
    print('Verified',repo,ref,len(data),flush=True);return dict(repo=repo,ref=ref,url=url,sha512=expected,bytes=len(data))
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:rows=list(pool.map(fetch,sources))
Path('build/g23-dependency-evaluation-2026-10-12/source-downloads.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
