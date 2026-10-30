"""Check bounded replay payloads before and after external GPU execution."""
from pathlib import Path
import hashlib
import re

MAX_BYTES=128*1024*1024


def verify_payloads(root: Path,manifest: dict,*,max_bytes: int=MAX_BYTES)->int:
    if type(max_bytes) is not int or not 0<max_bytes<=MAX_BYTES:
        raise ValueError('Invalid replay byte budget')
    if not isinstance(manifest,dict) or type(manifest.get('schemaVersion')) is not int or manifest['schemaVersion']!=1 or manifest.get('status')!='complete':
        raise ValueError('Unsupported or incomplete replay manifest')
    root=Path(root).resolve()
    total=0
    try:
        rows=[*manifest['constants'],manifest['tiles'],*manifest['textures']]
        for item in manifest['outputs']:rows.extend((item['initial'],item['reference']))
        for row in rows:
            name,size,digest=row['file'],row['bytes'],row['sha256']
            if not isinstance(name,str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*\.bin',name):
                raise ValueError('Replay payloads must be same-directory binary files')
            if type(size) is not int or size<=0 or size>max_bytes-total:
                raise ValueError('Replay payload length or total byte budget is invalid')
            if not isinstance(digest,str) or not re.fullmatch('[0-9a-f]{64}',digest):
                raise ValueError('Replay payload digest is invalid')
            path=root/name
            if path.is_symlink() or path.resolve().parent!=root or not path.is_file() or path.stat().st_size!=size:
                raise ValueError('Replay payload path or length differs from the manifest')
            with path.open('rb') as source:actual=hashlib.file_digest(source,'sha256').hexdigest()
            if actual!=digest:raise ValueError('Replay payload digest differs from the manifest')
            total+=size
    except (KeyError,TypeError,OSError) as error:
        raise ValueError('Invalid replay payload inventory') from error
    if 'bytes' in manifest and (type(manifest['bytes']) is not int or manifest['bytes']!=total):
        raise ValueError('Replay manifest total differs from its payloads')
    return total
