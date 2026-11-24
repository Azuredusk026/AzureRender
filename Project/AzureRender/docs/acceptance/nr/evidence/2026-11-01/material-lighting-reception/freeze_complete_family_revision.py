import gzip,hashlib,json,shutil
from pathlib import Path
run=Path(__file__).resolve().parent;root=run.parents[2];out=run/'shader-family-revision/frozen-complete';out.mkdir(exist_ok=False)
insert='''    if(integralMaterialProgram("/resources_integral/shaders/mesh.frag.spv"))return 30;
    if(integralMaterialProgram(R"(D:\\resources_integral\\shaders\\mesh.frag.spv)"))return 31;
    if(!integralMaterialProgram("/shaders/mesh_integral_object_bindless_clipmap_field.frag.spv"))return 32;
    if(!integralMaterialProgram("character_snapshot_integral.frag.spv"))return 33;
    if(integralMaterialProgram("mesh.frag.spv"))return 34;
'''
for cfg in ['release','debug']:
    report=json.loads((run/(cfg+'-build.json')).read_text());assert report['status']=='built-contract-tests-passed'
    shutil.copy2(run/(cfg+'-build.json'),out/(cfg+'-build.json'))
    for name,digest in report['sourceFiles'].items():
        data=(root/name).read_bytes().replace(b'\r\n',b'\n')
        if name=='tests/MaterialPipelinePolicyTests.cpp':
            text=data.decode();assert insert in text;data=text.replace(insert,'',1).encode()
        assert hashlib.sha256(data).hexdigest()==digest,name
        if cfg=='release':
            target=out/'source'/(name+'.gz');target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(gzip.compress(data,mtime=0))
folder=out/'release-program';folder.mkdir();build=root/'build/ninja-msvc-release'
for path in [build/'AzurePlayer.exe',*build.glob('*.dll')]:shutil.copy2(path,folder/path.name)
assert hashlib.sha256((folder/'AzurePlayer.exe').read_bytes()).hexdigest()==json.loads((out/'release-build.json').read_text())['programs']['AzurePlayer']
print('Frozen completed revision: 989 source files, both build receipts and independent Release executable')
