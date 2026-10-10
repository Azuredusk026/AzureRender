"""Add P5 delivery documents after frozen runtime regressions finish."""
from pathlib import Path
import json
import shutil
import subprocess

ROOT=Path.cwd().resolve()
WORK=ROOT/'build/implementation-2026-10-10'
for config in ('debug','release'):
    shutil.copy2(ROOT/f'build/ninja-msvc-{config}/build-provenance.json',WORK/f'p5-regression-source-{config}.json')
path=ROOT/'CMakeLists.txt'
addition='\ninstall(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/acceptance/p5/2026-10-10.md" "${CMAKE_CURRENT_SOURCE_DIR}/docs/acceptance/p5/physical-input.md" DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/acceptance/p5")\n'
text=path.read_text(encoding='utf-8')
assert 'docs/acceptance/p5/' not in text
with path.open('a',encoding='utf-8',newline='') as stream:stream.write(addition)
for config in ('debug','release'):
    script=WORK/f'p5-build-{config}.cmd'
    result=subprocess.run(['cmd.exe','/d','/c',str(script)],cwd=ROOT)
    assert result.returncode==0,(config,result.returncode)
    previous=json.loads((WORK/f'p5-regression-source-{config}.json').read_text(encoding='utf-8'))
    current=json.loads((ROOT/f'build/ninja-msvc-{config}/build-provenance.json').read_text(encoding='utf-8'))
    changed=[name for name,value in previous['source'].items() if current['source'].get(name)!=value]
    assert changed==['CMakeLists.txt'],changed
    assert previous['products']==current['products'],'Documentation install must preserve all tested binaries'
print('P5 install documents registered; both tested product sets unchanged',flush=True)
