"""Build independent Lua hosts with managed and Slang prototypes unavailable."""
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/p3'
build=root/'build/ninja-msvc-p3-off'
def run(name,args,msvc=False):
    command=([str(root/'tools/msvc_env.bat')]+args) if msvc else args
    with (work/(name+'.log')).open('w',encoding='utf-8') as log:
        result=subprocess.run(command,cwd=root,stdout=log,stderr=subprocess.STDOUT)
    if result.returncode:raise RuntimeError(name+': '+(work/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-6500:])
    print(name+' passed',flush=True)
run('modules-off-configure',['cmake','--preset','msvc-release','-B',str(build),
    '-DVCPKG_INSTALLED_DIR='+str(root/'build/ninja-msvc-release/vcpkg_installed'),
    '-DAZURE_ENABLE_MANAGED_SCRIPT_PROTOTYPE=OFF','-DAZURE_ENABLE_SHADER_MODULE_PROTOTYPE=OFF',
    '-DAZURE_DOTNET_EXECUTABLE='+str(work/'unavailable-dotnet.exe'),
    '-DAZURE_SLANG_COMPILER='+str(work/'unavailable-slangc.exe')],msvc=True)
run('modules-off-build',['cmake','--build',str(build),'--target','AzureRender','AzurePlayer',
    'AzureScriptRuntimeTests','AzureScriptEntityTests','AzureScriptBackendContractTests','AzureProjectTests','AzureModuleAssemblyTests','AzureResourceFrameTraceTests','AzureEngineContracts'],msvc=True)
discovery=subprocess.check_output(['ctest','--test-dir',str(build),'-C','Release','--show-only=json-v1'],cwd=root,text=True,encoding='utf-8')
(work/'modules-off-tests.json').write_text(discovery,encoding='utf-8')
tests=json.loads(discovery)['tests'];assert not any('Managed' in t['name'] or 'ShaderModule' in t['name'] or 'ShaderComposition' in t['name'] for t in tests)
run('modules-off-regression',['ctest','--test-dir',str(build),'-C','Release','-R',
    '^AzureEngine.(ScriptRuntime|ScriptEntities|ScriptBackendContract|Project|ModuleAssembly|ResourceFrameTrace|PlayerCli|ScriptPlayer|EditorHost|OptionalRuntimeTrace|ContractsSnapshot)$','--output-on-failure'])
metadata=json.loads((build/'runtime-build-Release.json').read_text(encoding='utf-8'))
assert metadata['scriptBackends']==['lua']
summary={'status':'passed','managedModuleEnabled':False,'shaderPrototypeEnabled':False,
    'dotnetCompilerUnavailable':True,'slangCompilerUnavailable':True,'scriptBackends':metadata['scriptBackends'],
    'independentBuild':str(build),'testNames':[t['name'] for t in tests]}
(work/'modules-off.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary),flush=True)
