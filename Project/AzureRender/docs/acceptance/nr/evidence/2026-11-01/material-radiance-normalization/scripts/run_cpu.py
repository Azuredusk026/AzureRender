import argparse,json,re,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;parser=argparse.ArgumentParser();parser.add_argument('configuration',choices=['release','debug']);args=parser.parse_args()
ctest='D:/Tools/VS2022BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe'
ready=json.loads((run/'release-ready.json').read_text());names=ready.get('tests') or ready['cpuTests'];assert len(names)==11
pattern='^('+'|'.join(re.escape(name) for name in names)+')$'
command=[ctest,'--test-dir',str(root/('build/ninja-msvc-'+args.configuration)),'-R',pattern,'-j','1','--output-on-failure','--no-tests=error']
result=subprocess.run(command,capture_output=True);output=result.stdout+result.stderr;(run/f'cpu-verified-{args.configuration}.log').write_bytes(output)
match=re.search(r'100% tests passed, 0 tests failed out of (\d+)',output.decode(errors='replace'))
report={'command':command,'exitCode':result.returncode,'selectedTests':names,'executedPassedCount':int(match[1]) if match else 0,'status':'passed' if result.returncode==0 and match and int(match[1])==len(names) else 'failed'}
(run/f'cpu-verified-{args.configuration}.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report));assert report['status']=='passed',output.decode(errors='replace')[-3000:]
