"""Read-only evidence size, identity and inbound-reference inventory."""
import hashlib
import os
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote, urlsplit

from code_query import safe_path

TEXT = {'.md', '.py', '.json', '.yml', '.yaml', '.cmake', '.txt', '.sh', '.ps1', '.bat'}


def inspect_directory(root, directory):
    root = Path(root).resolve()
    directory = Path(directory).absolute()
    path = safe_path(root, directory.relative_to(root))
    files = []
    seen = set()
    unique = 0
    logical = 0
    newest = path.stat().st_mtime_ns
    pending = [path]
    while pending:
        current = pending.pop()
        for child in sorted(current.iterdir()):
            safe_path(root, child.relative_to(root))
            # os.stat obtains the real Windows file identity; DirEntry.stat may
            # have st_ino=0 on supported Python/Windows versions.
            info = os.stat(child, follow_symlinks=False)
            newest = max(newest, info.st_mtime_ns)
            if child.is_dir():
                pending.append(child)
                continue
            digest = hashlib.sha256(child.read_bytes()).hexdigest()
            files.append({'path': child.relative_to(path).as_posix(), 'bytes': info.st_size,
                          'sha256': digest, 'mtimeNs': info.st_mtime_ns})
            logical += info.st_size
            identity = (info.st_dev, info.st_ino) if info.st_ino else str(child)
            if identity not in seen:
                unique += info.st_size
                seen.add(identity)
    return {'path': path.relative_to(root).as_posix(), 'files': len(files),
            'logicalBytes': logical, 'uniqueFileDataBytes': unique,
            'newestMtimeNs': newest, 'entries': sorted(files, key=lambda r:r['path'])}


def reference_graph(root, targets):
    root = Path(root).resolve()
    target_paths = [Path(p).resolve() for p in targets]
    listed = subprocess.check_output(['git','-C',str(root),'ls-files','-z','--cached','--others','--exclude-standard']).decode('utf-8')
    rows = set()
    for name in sorted(set(listed.split('\0'))- {''}):
        document = safe_path(root, name)
        if not document.is_file() or document.suffix.lower() not in TEXT:
            continue
        if document.stat().st_size > 8 * 1024 * 1024:
            continue
        text = document.read_text(encoding='utf-8',errors='replace')
        values = [(m.group(1).split()[0].strip('<>'), 'link') for m in re.finditer(r'\[[^\]]*\]\(([^)]+)\)',text)]
        values += [(m.group(1),'literal') for m in re.finditer(r'''["'`]([^"'`\r\n]{1,1000})["'`]''',text)
                   if '/' in m.group(1) or '\\' in m.group(1)]
        if document.suffix.lower() in {'.yml','.yaml'}:
            values += [(m.group(1),'navigation') for m in re.finditer(r':\s+([\w./-]+\.[\w]+)',text)]
        engine = root/'Project/AzureRender'
        bases = [document.parent, root, engine, engine/'docs']
        for value,kind in values:
            try:
                parsed = urlsplit(value.replace('\\','/'))
            except ValueError:
                continue
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            for base in bases:
                # Reference discovery is lexical and read-only. Avoid resolving
                # each manifest string through the filesystem; archive operations
                # independently enforce physical path/reparse boundaries.
                candidate = Path(os.path.abspath(base/unquote(parsed.path)))
                if root != candidate and root not in candidate.parents:
                    continue
                if any(candidate == t or t in candidate.parents for t in target_paths):
                    rows.add((name,candidate.relative_to(root).as_posix(),kind))
                    break
    return [{'document':d,'target':t,'kind':k} for d,t,k in sorted(rows)]


def evidence_inventory(root):
    root = Path(root).resolve()
    directories = [root/'Project/AzureRender/docs/acceptance'/phase for phase in ('p3','p5')]
    directories = [p for p in directories if p.is_dir()]
    refs = reference_graph(root,directories)
    rows = []
    for directory in directories:
        measured = inspect_directory(root,directory)
        prefix = measured['path']+'/'
        links = [r for r in refs if r['target'].startswith(prefix)]
        measured.pop('entries')
        measured.update(referenceCount=len(links), references=links[:30], referencesTruncated=len(links)>30)
        rows.append(measured)
    return {'schemaVersion':1,'method':'Logical bytes and file identity measured with os.stat; unique data is not allocated or reclaimable space.',
            'referenceCoverage':'Tracked and untracked nonignored text up to 8 MiB per file; explicit links and literal paths only. Dynamic paths require review.',
            'targets':rows}
