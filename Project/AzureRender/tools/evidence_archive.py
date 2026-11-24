"""Pack, verify and recover local evidence with exact-byte manifests."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import sys
import tempfile
import zipfile

from artifact_cleanup import remove_artifact
from code_query import ENGINE, safe_path
from evidence_inventory import inspect_directory

MAX_UNPACKED = 4 * 1024**3


def verify(path):
    with zipfile.ZipFile(path) as archive:
        infos = archive.infolist()
        names = [i.filename for i in infos]
        if len(names) != len(set(names)) or sum(i.file_size for i in infos) > MAX_UNPACKED:
            raise ValueError('Duplicate members or archive exceeds 4 GiB restore budget')
        for info in infos:
            member = PurePosixPath(info.filename)
            if member.is_absolute() or '..' in member.parts or '\\' in info.filename or ':' in info.filename or stat.S_ISLNK(info.external_attr >> 16):
                raise ValueError('Unsafe archive member')
        manifest = json.loads(archive.read('manifest.json'))
        if manifest.get('schemaVersion') != 1:
            raise ValueError('Unsupported archive schema')
        expected = {r['path'] for r in manifest['files']}
        if expected != set(names)-{'manifest.json'} or len(expected) != len(manifest['files']):
            raise ValueError('Archive membership differs from manifest')
        for row in manifest['files']:
            data = archive.read(row['path'])
            if len(data) != row['bytes'] or hashlib.sha256(data).hexdigest() != row['sha256']:
                raise ValueError('Archive hash mismatch: '+row['path'])
        return manifest


def pack(root, source_name, output_name, inputs=(), remove=False):
    source = safe_path(root,source_name)
    output = safe_path(root,output_name)
    if output.exists() or source == output or source in output.parents:
        raise ValueError('Archive output must be new and outside the source')
    if remove:
        runs = root/'build/runs'
        if source.parent != runs or not (source/'.finished').is_file() or (source/'.keep').exists():
            raise ValueError('Removal requires a finished, unpinned direct task directory')
    inventory = inspect_directory(root,source)
    if inventory['logicalBytes'] > MAX_UNPACKED:
        raise ValueError('Source exceeds restore budget')
    selected = {('payload/'+row['path']): source/row['path'] for row in inventory['entries']}
    for name in inputs:
        path = safe_path(root,name)
        if not path.is_file():
            raise ValueError('Input must name a regular file')
        selected['inputs/'+path.relative_to(root).as_posix()] = path
    commit = subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
    patch = subprocess.check_output(['git','-C',str(root),'diff','--binary','HEAD','--',*inputs]) if inputs else b''
    files = []
    snapshots = {name:path.stat().st_mtime_ns for name,path in selected.items()}
    output.parent.mkdir(parents=True,exist_ok=True)
    temporary = output.with_suffix(output.suffix+'.pending')
    if temporary.exists():
        raise ValueError('Archive temporary output already exists')
    try:
        with zipfile.ZipFile(temporary,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
            for name,path in sorted(selected.items()):
                data = path.read_bytes()
                if path.stat().st_mtime_ns != snapshots[name]:
                    raise ValueError('Input changed during archive creation')
                archive.writestr(name,data)
                files.append({'path':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
            archive.writestr('working-tree.patch',patch)
            files.append({'path':'working-tree.patch','bytes':len(patch),'sha256':hashlib.sha256(patch).hexdigest()})
            manifest = {'schemaVersion':1,'source':source_name,'sourceCommit':commit,'files':files}
            archive.writestr('manifest.json',json.dumps(manifest,ensure_ascii=False,sort_keys=True))
        verify(temporary)
        if inspect_directory(root,source) != inventory or any(p.stat().st_mtime_ns != snapshots[n] for n,p in selected.items()):
            raise ValueError('Source changed after archive verification')
        temporary.rename(output)
        if remove:
            if (source/'.keep').exists() or not (source/'.finished').is_file():
                raise ValueError('Source retention markers changed')
            repo = Path(subprocess.check_output(['git','-C',str(root),'rev-parse','--show-toplevel'],text=True).strip())
            listed = subprocess.check_output(['git','-C',str(repo),'ls-files','-z']).decode('utf-8').split('\0')
            tracked = {(repo/p).resolve() for p in listed if p}
            remove_artifact(root/'build/runs',source,tracked,apply=True)
        return {'status':'passed','archive':output_name,'files':len(files),'logicalBytes':inventory['logicalBytes'],
                'archiveBytes':output.stat().st_size,'sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'removedSource':remove,'sourceCommit':commit}
    finally:
        if temporary.exists():temporary.unlink()


def restore(root, archive_name, output_name):
    path = safe_path(root,archive_name)
    output = safe_path(root,output_name)
    if output.exists():
        raise ValueError('Recovery destination must be new')
    manifest = verify(path)
    output.parent.mkdir(parents=True,exist_ok=True)
    output.mkdir()
    with zipfile.ZipFile(path) as archive:
        for row in manifest['files']:
            target = safe_path(output,row['path'])
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(archive.read(row['path']))
            if hashlib.sha256(target.read_bytes()).hexdigest() != row['sha256']:
                raise ValueError('Recovery hash mismatch')
    return {'status':'passed','files':len(manifest['files']),'sourceCommit':manifest['sourceCommit']}


def main():
    p = argparse.ArgumentParser(__doc__)
    p.add_argument('--root',type=Path,default=ENGINE)
    sub = p.add_subparsers(dest='operation',required=True)
    for name in ('pack','verify','restore'):
        c = sub.add_parser(name)
        if name == 'pack':
            c.add_argument('--source',required=True)
            c.add_argument('--output',required=True)
            c.add_argument('--input',action='append',default=[])
            c.add_argument('--remove-source',action='store_true')
        else:
            c.add_argument('--archive',required=True)
            if name == 'restore':c.add_argument('--output',required=True)
    args = p.parse_args()
    try:
        root = args.root.resolve()
        if args.operation == 'pack':result = pack(root,args.source,args.output,args.input,args.remove_source)
        elif args.operation == 'restore':result = restore(root,args.archive,args.output)
        else:
            manifest = verify(safe_path(root,args.archive))
            result = {'status':'passed','files':len(manifest['files']),'sourceCommit':manifest['sourceCommit']}
        print(json.dumps(result,ensure_ascii=False))
        return 0
    except (OSError,ValueError,KeyError,zipfile.BadZipFile,subprocess.CalledProcessError) as error:
        print(json.dumps({'status':'error','error':str(error)},ensure_ascii=False))
        return 2


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    raise SystemExit(main())
