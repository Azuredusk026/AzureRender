"""Archive stage verification without rerunning completed tests."""
import datetime,argparse,hashlib,json,pathlib,re,shutil,subprocess
p=argparse.ArgumentParser();p.add_argument('--phase',required=True);p.add_argument('--date',default=datetime.date.today().isoformat());args=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1];phase=args.phase;build=root/'build'/phase;e=root/'docs/acceptance'/phase/'evidence'/args.date;e.mkdir(parents=True,exist_ok=True)
(e/'.gitattributes').write_bytes(b'* -text whitespace=trailing-space,space-before-tab,cr-at-eol\n')
checks={}
for config in ['debug','release']:
 text=(build/f'{config}-test.log').read_text(encoding='utf-8',errors='replace')
 match=re.search(r'100% tests passed, 0 tests failed out of (\d+)',text);assert match,f'{config} tests failed'
 checks[config]={'passed':int(match[1])}
 for kind in ['build','test']:
  source=build/f'{config}-{kind}.log';data=b'\n'.join(line.rstrip(b' \t\r') for line in source.read_bytes().splitlines()).rstrip(b'\n')+b'\n';(e/source.name).write_bytes(data)
source={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes().replace(b'\r\n',b'\n')).hexdigest() for base in ['src','tools','tests','assets_public'] for p in (root/base).rglob('*') if p.is_file() and p.suffix in ['.cpp','.hpp','.py','.cmake','.ps1','.json','.lua','.azurelevel','.azureprefab','.azureproject','.azmeta']}
for name in ['CMakeLists.txt','vcpkg.json']:source[name]=hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()
manifest={'schemaVersion':1,'phase':phase.upper(),'status':'complete','parentCommit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'checks':checks,'sourceHashNormalization':'LF','sourceSha256':source,'binarySha256':{name:hashlib.sha256((root/'build/ninja-msvc-release'/name).read_bytes()).hexdigest() for name in ['AzureRender.exe','AzurePlayer.exe']},'evidenceSha256':{p.relative_to(e).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in e.rglob('*') if p.is_file() and p.name!='manifest.json'}}
(e/'manifest.json').write_bytes((json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode())
print(f'{phase}: Debug {checks["debug"]["passed"]}, Release {checks["release"]["passed"]}; evidence archived')
