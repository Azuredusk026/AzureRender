from pathlib import Path
import sys,json,hashlib,os
sys.path.insert(0,str(Path('tools').resolve()))
from run_playable_performance import fixture,run
from build_provenance import inputs,verify
root=Path.cwd();out=root/'build/p5-p6-closure-2026-10-10'
verify(root,json.loads((root/'build/ninja-msvc-release/build-provenance.json').read_text()))
frozen=inputs(root)
quality=fixture(out/'baseline-project')
os.environ['AZURERENDER_EDITOR_CONFIG']=str(out/'baseline-config')
result=run(root/'build/ninja-msvc-release/AzureRender.exe',out/'baseline-project/project.azureproject',out/'baseline-editor',editor=True)
result['sourceHash']=hashlib.sha256(json.dumps(frozen,sort_keys=True).encode()).hexdigest()
result['sourceCommit']='60f189c'
result['quality']=quality
assert inputs(root)==frozen
(out/'baseline-editor-summary.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print('growth',result['stableWorkingSetGrowth'],'failures',result['budgetFailures'])
