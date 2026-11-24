"""Exercise project CLI contracts using the real standalone Player."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import base64
import struct

def verify(executable):
    executable=Path(executable).resolve()
    temporary_root=Path(__file__).resolve().parents[1]/'build/runs'
    temporary_root.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="azure-player-",dir=temporary_root) as directory:
        root=Path(directory)
        def run(*args,success=True):
            result=subprocess.run([str(executable),*map(str,args)],cwd=root,capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=30)
            if (result.returncode==0)!=success:
                raise AssertionError(f"{args}: {result.stdout}{result.stderr}")
            return result.stdout+result.stderr
        assert "AzurePlayer" in run("--version")
        run("--create-project",root/"original")
        run("--create-project",root/"original",success=False)
        (root/"original").rename(root/"moved")
        project=root/"moved/project.azureproject"
        assert "validation passed" in run("--project",project,"--check-project")
        run("--check-project",success=False)
        run("--project",project,"--editor",root/"editor.azscene",success=False)
        run("--project",success=False)
        run("--project",project,"--unknown",success=False)
        original=json.loads(project.read_text())
        document=dict(original,scripting={'schemaVersion':1,'backend':'unavailable-backend'});project.write_text(json.dumps(document))
        assert "backend unavailable" in run("--project",project,"--check-project",success=False)
        document=dict(original,scripting={'schemaVersion':1,'backend':'coreclr','module':'assets:/missing.dll','runtimeConfig':'assets:/missing.json'});project.write_text(json.dumps(document))
        run("--project",project,"--check-project",success=False)
        document=dict(original,schemaVersion=99);project.write_text(json.dumps(document))
        assert "schemaVersion" in run("--project",project,"--check-project",success=False)
        document=dict(original,mounts=original["mounts"]*2);project.write_text(json.dumps(document))
        assert "Duplicate" in run("--project",project,"--check-project",success=False)
        project.write_text(json.dumps(original))
        asset_root=root/'moved/assets'
        asset_root.mkdir(exist_ok=True)
        vertices=struct.pack('<9f',0,0,0,1,0,0,0,1,0)
        normals=struct.pack('<9f',*([0,0,1]*3))
        uv=struct.pack('<6f',0,0,1,0,0,1)
        tangent=struct.pack('<12f',*([1,0,0,1]*3))
        indices=struct.pack('<3H',0,1,2)
        payload=vertices+normals+uv+tangent+indices
        probe=dict(asset=dict(version='2.0'),scene=0,scenes=[dict(nodes=[0])],nodes=[dict(mesh=0)],
            meshes=[dict(weights=[0,0],primitives=[dict(attributes=dict(POSITION=0,NORMAL=1,TEXCOORD_0=2,TANGENT=3),indices=4,material=0)])],
            materials=[dict(pbrMetallicRoughness=dict(baseColorFactor=[1,1,1,1]))],
            buffers=[dict(byteLength=len(payload),uri='data:application/octet-stream;base64,'+base64.b64encode(payload).decode())],
            bufferViews=[dict(buffer=0,byteOffset=offset,byteLength=size) for offset,size in [(0,36),(36,36),(72,24),(96,48),(144,6)]],
            accessors=[dict(bufferView=i,componentType=5123 if i==4 else 5126,count=3,type=kind) for i,kind in enumerate(['VEC3','VEC3','VEC2','VEC4','SCALAR'])])
        bad=asset_root/'unused baked probe.gltf';bad.write_text(json.dumps(probe))
        diagnostic=run('--project',project,'--check-project',success=False)
        assert 'Morph default weight count mismatch' in diagnostic,diagnostic
        assert bad.name in diagnostic,diagnostic
        probe['meshes'][0].pop('weights');bad.write_text(json.dumps(probe))
        assert 'validation passed' in run('--project',project,'--check-project')
        print("Player CLI: creation, migration, validation and error contracts passed")

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("--executable",required=True)
    verify(parser.parse_args().executable)
