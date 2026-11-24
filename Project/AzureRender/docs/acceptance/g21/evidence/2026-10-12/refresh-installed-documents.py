from pathlib import Path
import json,subprocess
root=Path.cwd().resolve();out=root/'build/g21-dependency-evaluation-2026-10-12';install=out/'Engine unavailable'
def run(name,args):
    p=subprocess.run(list(map(str,args)),capture_output=True,encoding='utf-8',errors='replace',timeout=300)
    (out/(name+'.log')).write_bytes((p.stdout+p.stderr).encode());assert p.returncode==0,(name,p.stdout,p.stderr)
run('install-final',['cmake','--install',root/'build/ninja-msvc-release','--config','Release','--prefix',install])
for relative in ('runtime/asset-exchange.md','plans/asset-format-manifest.json','plans/azure-engine-plan.md'):
    assert (install/'share/AzureRender/docs'/relative).read_bytes()==(root/'docs'/relative).read_bytes()
for device in ('nvidia','intel'):
    cap=json.loads((out/(device+'-capture/gpu_capabilities.json')).read_text(encoding='utf-8'));assert ('Intel' if device=='intel' else 'NVIDIA') in cap['device_name']
run('install-final-manifest',['cmake','-DINSTALL_DIR='+install.as_posix(),'-DOUTPUT_FILE='+str(out/'install-manifest.json'),'-P',root/'tools/write_install_manifest.cmake'])
run('install-final-verify',['cmake','-DINSTALL_DIR='+install.as_posix(),'-DMANIFEST_FILE='+str(out/'install-manifest.json'),'-P',root/'tools/verify_install_manifest.cmake'])
p=out/'delivery-summary.json';summary=json.loads(p.read_text(encoding='utf-8'));summary['fileCount']=json.loads((out/'install-manifest.json').read_text(encoding='utf-8'))['file_count'];summary['installedDocumentsMatch']=True;p.write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
print('Final installed guide, plans, inventory and actual dual-device identities verified')
