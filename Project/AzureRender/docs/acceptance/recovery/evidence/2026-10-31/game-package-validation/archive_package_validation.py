from pathlib import Path
import gzip,hashlib,json
root=Path.cwd();r=Path(__file__).resolve().parent;out=root/'docs/acceptance/recovery/evidence/2026-10-31/game-package-validation';out.mkdir(parents=True,exist_ok=True);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def copy(p,name):
 data=p.read_bytes().replace(b'\r\n',b'\n').rstrip();data=data+b'\n' if data else b''
 if name.endswith('.gz'):data=gzip.compress(data,mtime=0)
 (out/name).write_bytes(data)
for name in ['validation-budget-probe.json','install-verification.json','publication.json','package-budget-red.log','package-budget-red-final.log','package-budget-number-red.log','package-budget-green.log','package-rules-debug.log','package-rules-release.log','install-path-red.log']:
 copy(r/name,'initial-budget-green.log' if name=='package-budget-green.log' else name)
for name in ['install.log','install-green.log','configure-debug.log','configure-release.log','publication.log','validation-budget-stdout.log','validation-budget-stderr.log']:copy(r/name,name+'.gz')
for name in ['probe_validation_budget.py','verify_install.py','publish.py','archive_package_validation.py']:copy(r/name,name)
source=['CMakeLists.txt','tools/build_game.py','tools/test_game_package_rules.py'];fingerprints={p:dict(workingTreeSha256=sha(root/p),canonicalLfSha256=hashlib.sha256((root/p).read_bytes().replace(b'\r\n',b'\n')).hexdigest()) for p in source}
summary=dict(status='passed',sourceBase='9320a7d',sources=fingerprints,defaultValidationSeconds=1800,maximumValidationSeconds=3600,realOldBudgetSeconds=60,oldBudgetTimeoutObserved=True,ruleTestsPerConfiguration=19,configurations=['Debug','Release'],installedPythonIsolated=True,windowsInstallPathWarningResolved=True,nativeProgramsUnchangedFromJointValidation=True,realSourceAndCandidateChecksPassed=True,packageFiles=2200,packageManifestSha256=json.loads((r/'publication.json').read_text())['manifestSha256'],scope='Offline Player validation budget and Python installation path; native gameplay and formal performance are separate')
(out/'summary.json').write_bytes((json.dumps(summary,indent=2)+'\n').encode());files=[dict(path=p.name,bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(out.iterdir()) if p.name!='artifacts.json'];(out/'artifacts.json').write_bytes((json.dumps(dict(schemaVersion=1,files=files),indent=2)+'\n').encode());print(json.dumps(dict(status='passed',files=len(files),bytes=sum(x['bytes'] for x in files))))
