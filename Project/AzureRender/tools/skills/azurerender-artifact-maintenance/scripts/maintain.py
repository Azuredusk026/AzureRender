"""Small deployed entrypoint; the project owns lifecycle implementation."""
import argparse
from pathlib import Path
import subprocess
import sys

def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('mode',choices=['status','maintain','daily','resume','restore'])
    parser.add_argument('--engine',type=Path,default=Path('D:/Projects/FYP/Project/AzureRender'))
    parser.add_argument('--apply',action='store_true')
    parser.add_argument('--run')
    args=parser.parse_args();engine=args.engine.absolute()
    for path in (engine,*engine.parents):
        if path.exists() and (path.is_symlink() or getattr(path.lstat(),'st_file_attributes',0)&1024):
            parser.error('Project path contains a link/reparse point')
    backend=engine/'tools/artifact_maintenance.py'
    if engine.name!='AzureRender' or not backend.is_file() or not (engine/'CMakeLists.txt').is_file():
        parser.error('Expected the AzureRender source project')
    python=engine/'build/reinstall-environment/venv/Scripts/python.exe'
    command=[str(python) if python.is_file() else sys.executable,'-X','utf8',str(backend),args.mode]
    if args.apply:command.append('--apply')
    if args.run:command.extend(['--run',args.run])
    return subprocess.run(command,cwd=engine).returncode

if __name__=='__main__':raise SystemExit(main())
