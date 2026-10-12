"""Publish the established alternating fixture and execute real long runs."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path
ROOT=Path.cwd()
OUT=ROOT/'build/p8-format-closure-2026-10-12'
sys.path.insert(0,str(ROOT/'tools'))
from test_level_cycles import prepare
from build_game import build_game,verify_package
from build_provenance import inputs,verify
proof=json.loads((ROOT/'build/ninja-msvc-release/build-provenance.json').read_text(encoding='utf-8'))
verify(ROOT,proof)
frozen=inputs(ROOT)
source=OUT/'standard-alternating-source'
standard=OUT/'Moved standard alternating'
prepare(source)
build_game(source/'project.azureproject',OUT/'arena-delivery/Moved Engine',standard)
source.rename(OUT/'Unavailable standard alternating source')
verify_package(standard)
assert (standard/'game/assets/alternate.azurelevel').is_file()
platform=OUT/'arena-delivery/Moved exploration'
assert (platform/'game/assets/platformer/platformer.azurelevel').is_file()
rows=[]
for name,tool,package in [('standard','run_playable_long_run.py',standard),
    ('platformer','run_platformer_long_run.py',platform)]:
    output=OUT/(name+'-long-run-final')
    print('Starting actual 1800 seconds:',name,flush=True)
    with (OUT/(name+'-long-run-final.log')).open('w',encoding='utf-8') as log:
        result=subprocess.run([sys.executable,'-X','utf8',str(ROOT/'tools'/tool),
            '--package',str(package),'--output',str(output),'--seconds','1800'],stdout=log,stderr=subprocess.STDOUT)
    assert inputs(ROOT)==frozen
    verify_package(package)
    rows.append(dict(workload=name,exitCode=result.returncode,evidence=str(output),
        runtimeHash=hashlib.sha256((package/'bin/AzurePlayer.exe').read_bytes()).hexdigest(),
        packageManifestHash=hashlib.sha256((package/'game_manifest.json').read_bytes()).hexdigest()))
    (OUT/'long-run-final-status.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
    print(name,'long run exit:',result.returncode,flush=True)
