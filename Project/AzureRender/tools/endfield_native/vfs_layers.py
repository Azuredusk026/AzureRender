"""Read an explicit low-to-high catalog stack without modifying source layers.

Each selected catalog describes a complete group. Chunk bytes may reside in
any declared layer. File checksums come from the selected catalog; missing or
corrupt updates never select an older virtual-file entry.
"""
import hashlib
from contextlib import closing
from io import BytesIO
from pathlib import Path
import re
import sqlite3


class LayeredVfs:
    def __init__(self,roots,backend):
        self.roots=[Path(p).resolve(strict=True) for p in roots]
        if not self.roots or len(set(self.roots))!=len(self.roots) or any(not p.is_dir() for p in self.roots):
            raise ValueError('VFS layers must be unique existing directories')
        self.backend=backend;self.groups={};self.catalogs=[];self.reads=[]
        self.key=bytes.fromhex(backend.DEFAULT_VFS_KEY_HEX)

    def _unchanged(self,record):
        identity=record['group'].group_hash.upper()
        current=[root/identity/(identity+'.blc') for root in self.roots]
        current=[p for p in current if p.is_file()]
        if not current or current[-1]!=record['path']:
            raise ValueError('VFS catalog selection changed during source snapshot: '+identity)
        path=record['path'];stat=path.stat()
        if (stat.st_size,stat.st_mtime_ns)!=record['stamp']:
            raise ValueError('VFS catalog changed during source snapshot: '+str(path))

    def group(self,group_hash):
        if not isinstance(group_hash,str) or not re.fullmatch(r'[0-9a-fA-F]{8}',group_hash):
            raise ValueError('invalid VFS group identity')
        identity=group_hash.upper()
        if identity in self.groups:
            record=self.groups[identity];self._unchanged(record);return record
        candidates=[(i,root/identity/(identity+'.blc')) for i,root in enumerate(self.roots)]
        candidates=[(i,p) for i,p in candidates if p.is_file()]
        if not candidates:raise ValueError('VFS catalog absent: '+identity)
        layer,path=candidates[-1];before=path.stat();raw=path.read_bytes();after=path.stat()
        if (before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns):
            raise ValueError('VFS catalog changed while reading: '+str(path))
        group=self.backend.parse_blc(self.backend.decrypt_blc(raw,self.key),expected_group_hash=identity)
        if group.group_hash.upper()!=identity:raise ValueError('VFS catalog group identity mismatch')
        files={}
        for item in group.iter_files():
            name=item.file_name.casefold()
            if name in files:raise ValueError('duplicate virtual path in VFS catalog: '+item.file_name)
            if not re.fullmatch(r'[0-9a-fA-F]{32}',item.file_chunk_md5_name):raise ValueError('invalid VFS chunk identity')
            files[name]=item
        chunks={x.md5_name.upper():x for x in group.chunks}
        if len(chunks)!=len(group.chunks):raise ValueError('duplicate VFS chunk identity')
        record=dict(path=path,stamp=(after.st_size,after.st_mtime_ns),group=group,files=files,chunks=chunks)
        self.groups[identity]=record
        self.catalogs.append(dict(group=identity,catalog_layer=layer,path=str(path),sha256=hashlib.sha256(raw).hexdigest(),files=len(files)))
        return record

    def _file(self,group_hash,virtual_path):
        record=self.group(group_hash)
        if not isinstance(virtual_path,str):raise ValueError('virtual path must be text')
        item=record['files'].get(virtual_path.casefold())
        if item is None:raise ValueError('virtual file absent from selected catalog: '+virtual_path)
        name=item.file_chunk_md5_name.upper()
        chunk=record['chunks'].get(name)
        if chunk is None:raise ValueError('virtual file references undeclared chunk: '+name)
        paths=[(i,root/group_hash.upper()/(name+'.chk')) for i,root in enumerate(self.roots)]
        paths=[(i,p) for i,p in paths if p.is_file()]
        if not paths:raise ValueError('VFS chunk missing from all source layers: '+name)
        layer,path=paths[-1]
        if path.stat().st_size!=chunk.length:raise ValueError('VFS chunk size differs from catalog: '+str(path))
        return record,item,layer,path

    def _copy(self,group_hash,virtual_path,target):
        record,item,layer,path=self._file(group_hash,virtual_path)
        with path.open('rb') as source:
            digest=self.backend.copy_virtual_file(source,target,item,record['group'].version,self.key)
        if digest.upper()!=item.file_data_md5.upper():raise ValueError('VFS payload checksum mismatch: '+virtual_path)
        self._unchanged(record)
        self.reads.append(dict(group=group_hash.upper(),virtual_path=item.file_name,chunk_layer=layer,
            chunk_path=str(path),data_md5=item.file_data_md5.upper(),bytes=item.length))

    def read(self,group_hash,virtual_path):
        target=BytesIO();self._copy(group_hash,virtual_path,target);return target.getvalue()

    def output_path(self,target):
        path=Path(target).resolve()
        if any(path==root or path.is_relative_to(root) for root in self.roots):
            raise ValueError('output must be outside all source VFS layers')
        return path

    def copy(self,group_hash,virtual_path,target):
        target=self.output_path(target)
        # Validate before creating the output; failed checksum artifacts remain
        # local diagnostic data and are never recorded as successful reads.
        self._file(group_hash,virtual_path)
        with target.open('xb') as stream:self._copy(group_hash,virtual_path,stream)

    def receipt(self):
        return dict(format='EndfieldLayeredVfsReceipt/1',priority='caller_explicit_low_to_high',
            layers=[str(p) for p in self.roots],catalogs=list(self.catalogs),reads=list(self.reads))

    def extract_bundles(self,database,logical_names,output,progress=None,cancelled=None):
        root=self.output_path(output)
        if root.exists():raise FileExistsError(root)
        wanted={str(x).casefold() for x in logical_names}
        source=Path(database).resolve(strict=True)
        with closing(sqlite3.connect(source.as_uri()+'?mode=ro',uri=True)) as connection:
            connection.create_function('requested_bundle',1,lambda name:str(name).casefold() in wanted,deterministic=True)
            selected=list(connection.execute('SELECT logical_name,group_hash,virtual_path FROM bundles WHERE requested_bundle(logical_name) ORDER BY logical_name'))
        if wanted-{str(row[0]).casefold() for row in selected}:raise ValueError('requested bundle absent from resource database')
        destinations=[]
        for logical,group,path in selected:
            parts=str(logical).replace('\\','/').split('/')
            if len(parts)!=2 or parts[0] not in ('main','initial') or not parts[1] or parts[1] in ('.','..') or ':' in parts[1]:
                raise ValueError('invalid logical bundle identity')
            destinations.append((group,path,root/parts[0]/parts[1]))
        root.mkdir(parents=True);size=0
        for i,(group,path,target) in enumerate(destinations):
            if cancelled and cancelled():raise ValueError('layered bundle extraction cancelled')
            target.parent.mkdir(parents=True,exist_ok=True);self.copy(group,path,target);size+=target.stat().st_size
            if progress:progress('scene_candidate_bundles',(i+1)/max(1,len(destinations)),'Extracted layered source bundle',dict(virtualPath=path))
        return dict(format='EndfieldCandidateBundleSet/1',requested_count=str(len(wanted)),extracted_count=str(len(destinations)),bytes=str(size))

    def install(self,resources):
        original_group=resources.load_group;original_read=resources.read_virtual_file;original_extract=resources.extract_candidate_bundles
        def selected(root):return Path(root).resolve() in self.roots
        resources.load_group=lambda root,group:self.group(group)['group'] if selected(root) else original_group(root,group)
        resources.read_virtual_file=lambda root,group,path:self.read(group,path) if selected(root) else original_read(root,group,path)
        def extract(database,root,names,output,*,progress=None,cancelled=None):
            if selected(root):return self.extract_bundles(database,names,output,progress,cancelled)
            return original_extract(database,root,names,output,progress=progress,cancelled=cancelled)
        resources.extract_candidate_bundles=extract
