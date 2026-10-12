"""Install the builder's standard-library-only Python runtime beside engine tools."""
import argparse,shutil,sys
from pathlib import Path

def install(destination):
    source=Path(sys.base_prefix);destination.mkdir(parents=True,exist_ok=True)
    for pattern in ('python.exe','python*.dll','python*.zip','vcruntime*.dll','LICENSE.txt'):
        for file in source.glob(pattern):shutil.copy2(file,destination/file.name)
    for name in ('DLLs','Lib'):
        if (source/name).is_dir():
            shutil.copytree(source/name,destination/name,dirs_exist_ok=True,ignore=shutil.ignore_patterns('site-packages','__pycache__','test','tests','idlelib','turtledemo','ensurepip'))
    tag=f'python{sys.version_info.major}{sys.version_info.minor}'
    (destination/(tag+'._pth')).write_text(f'{tag}.zip\nLib\nDLLs\n.\n',encoding='utf-8')
    if not (destination/'python.exe').is_file():raise RuntimeError('A Windows Python installation is required for the installed builder')
    print('Installed isolated Python builder runtime:',destination)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--destination',type=Path,required=True);args=parser.parse_args();install(args.destination)
