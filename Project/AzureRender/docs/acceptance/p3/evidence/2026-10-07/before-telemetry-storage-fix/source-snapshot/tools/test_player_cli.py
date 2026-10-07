"""Exercise project CLI contracts using the real standalone Player."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

def verify(executable):
    executable=Path(executable).resolve()
    with tempfile.TemporaryDirectory(prefix="azure-player-") as directory:
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
        print("Player CLI: creation, migration, validation and error contracts passed")

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("--executable",required=True)
    verify(parser.parse_args().executable)
