"""Build optional trusted CoreCLR and NativeAOT modules outside source roots."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import time
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]

def project(path,assembly,sources,properties=None,reference=None):
    root=ET.Element('Project',Sdk='Microsoft.NET.Sdk');group=ET.SubElement(root,'PropertyGroup')
    values={'TargetFramework':'net9.0','AssemblyName':assembly,'AllowUnsafeBlocks':'true',
        'Nullable':'enable','ImplicitUsings':'enable','TreatWarningsAsErrors':'true',
        'EnableDefaultCompileItems':'false','GenerateRuntimeConfigurationFiles':'true',
        'JsonSerializerIsReflectionEnabledByDefault':'false'}
    values.update(properties or {})
    for name,value in values.items():ET.SubElement(group,name).text=value
    items=ET.SubElement(root,'ItemGroup')
    for source in sources:ET.SubElement(items,'Compile',Include=str(source))
    if reference:
        item=ET.SubElement(items,'Reference',Include='Azure.Engine');ET.SubElement(item,'HintPath').text=str(reference)
    path.parent.mkdir(parents=True,exist_ok=True);ET.ElementTree(root).write(path,encoding='utf-8',xml_declaration=True)

def build(output):
    output=output.resolve();output.mkdir(parents=True,exist_ok=True)
    dotnet=shutil.which('dotnet')
    if not dotnet:raise RuntimeError('.NET 9 SDK required for managed prototypes')
    sdk=subprocess.check_output([dotnet,'--version'],text=True).strip()
    if not sdk.startswith('9.'):raise RuntimeError('Managed prototypes require .NET 9 SDK')
    subprocess.run([shutil.which('python') or 'python',str(ROOT/'tools/generate_script_bindings.py'),'--check'],check=True)
    description=json.loads((ROOT/'schemas/script_bindings.json').read_text())
    digest=hashlib.sha256(json.dumps(description,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    generated=output/'generated';generated.mkdir(exist_ok=True)
    types=json.loads((ROOT/'managed/SampleScripts/types.json').read_text())
    if set(types)!={'schemaVersion','types'} or type(types['schemaVersion']) is not int or types['schemaVersion']!=1:
        raise ValueError('Invalid managed type manifest version')
    if not types['types'] or len(types['types'])>128 or len(set(types['types']))!=len(types['types']):raise ValueError('Invalid managed type count')
    for name in types['types']:
        if not isinstance(name,str) or not re.fullmatch(r'[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)+',name):raise ValueError('Invalid managed type name')
    registry=generated/'NativeTypes.g.cs'
    registry.write_text('namespace Azure.Engine; internal static class NativeTypes { internal static ScriptBehaviour Create(string type)=>type switch {\n'+
        ''.join('"'+name+'"=>new global::'+name+'(),\n' for name in types['types'])+
        '_=>throw new System.ArgumentException("NativeAOT script type is absent from the compiled manifest") }; }\n')
    sources=sorted((ROOT/'managed/EngineBindings').glob('*.cs'))
    samples=sorted((ROOT/'managed/SampleScripts').glob('*.cs'))
    results=[]
    def run(backend,args):
        started=time.perf_counter();subprocess.run([dotnet,*args],check=True)
        results.append({'backend':backend,'buildSeconds':time.perf_counter()-started})
    core=output/'coreclr';native=output/'nativeaot'
    core_project=ROOT/'managed/EngineBindings/AzureEngineBindings.csproj'
    run('coreclr',['build',str(core_project),'-c','Release','-o',str(core),'--artifacts-path',str(output/'artifacts/core'),'--nologo'])
    sample_project=ROOT/'managed/SampleScripts/AzureSamples.csproj'
    run('coreclr-samples',['build',str(sample_project),'-c','Release','-o',str(core),'--artifacts-path',str(output/'artifacts/samples'),
        '-p:EngineAssembly='+str(core/'Azure.Engine.dll'),'--nologo'])
    native_project=output/'projects/native/Azure.Engine.Native.csproj'
    project(native_project,'Azure.Engine.Native',sources+samples+[registry],{'PublishAot':'true','NativeLib':'Shared',
        'SelfContained':'true','RuntimeIdentifier':'win-x64','RuntimeFrameworkVersion':'9.0.11','DefineConstants':'AZURE_NATIVEAOT'})
    run('nativeaot',['publish',str(native_project),'-c','Release','-o',str(native),'--nologo'])
    for result in results:
        directory=native if result['backend']=='nativeaot' else core
        result['bytes']=sum(p.stat().st_size for p in directory.iterdir() if p.is_file() and p.suffix in {'.dll','.json','.pdb','.lib'})
        result['distributionBytes']=sum(p.stat().st_size for p in directory.iterdir() if p.is_file() and p.suffix in {'.dll','.json'})
    manifest={'schemaVersion':1,'sdk':sdk,'bindingHash':digest,'types':types['types'],'measurements':results,
        'files':{str(p.relative_to(output)).replace('\\','/'):{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
            for directory in (core,native) for p in directory.iterdir() if p.is_file() and p.suffix in {'.dll','.json','.pdb','.lib'}}}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (output/'complete.stamp').write_text(digest+'\n')
    print(json.dumps(manifest['measurements']))

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--output',type=Path,required=True)
    build(parser.parse_args().output)
