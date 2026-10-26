"""Incremental syntax maps; deliberately does not claim compiler-level references."""
import hashlib
from concurrent.futures import ThreadPoolExecutor, TimeoutError as FutureTimeout
from importlib import metadata
import json
import os
from pathlib import Path
import re
import sys
import time
import uuid

from code_query import ENGINE, QueryError, base_payload, safe_path, scope_paths, source_files

PACKAGES = {'cpp': 'tree_sitter_cpp', 'python': 'tree_sitter_python', 'csharp': 'tree_sitter_c_sharp'}
EXTENSIONS = {'.cpp':'cpp','.hpp':'cpp','.h':'cpp','.c':'cpp','.cc':'cpp','.inc':'cpp',
              '.frag':'cpp','.vert':'cpp','.comp':'cpp','.glsl':'cpp','.geom':'cpp',
              '.py':'python','.cs':'csharp'}
KINDS = {'class_specifier':'class','struct_specifier':'struct','enum_specifier':'enum',
         'alias_declaration':'alias','function_definition':'function','function_declarator':'declaration',
         'class_definition':'class','class_declaration':'class','struct_declaration':'struct',
         'interface_declaration':'interface','enum_declaration':'enum','method_declaration':'method',
         'constructor_declaration':'constructor','property_declaration':'property'}


def backend():
    bundled = ENGINE/'build/qa-python/code-intelligence'
    if bundled.is_dir() and str(bundled) not in sys.path:
        sys.path.insert(0,str(bundled))
    try:
        from tree_sitter import Language, Parser
        import importlib
        parsers = {name:Parser(Language(importlib.import_module(package).language())) for name,package in PACKAGES.items()}
        versions = {name:metadata.version(name) for name in ('tree-sitter','tree-sitter-cpp','tree-sitter-python','tree-sitter-c-sharp')}
        return parsers,versions
    except (ImportError,ValueError,TypeError) as error:
        raise QueryError('Install tools/requirements-code-intelligence.txt into build/qa-python/code-intelligence; text search remains available') from error


def node_text(node, raw):
    return raw[node.start_byte:node.end_byte].decode('utf-8','replace')


def declaration_name(node, raw):
    direct = node.child_by_field_name('name')
    if direct:
        return node_text(direct,raw)
    declarator = node.child_by_field_name('declarator')
    while declarator:
        next_node = declarator.child_by_field_name('declarator')
        if next_node is None:
            return node_text(declarator,raw)
        declarator = next_node
    return ''


def parse_file(parser, raw, language):
    tree = parser.parse(raw)
    symbols = []
    pending = [tree.root_node]
    while pending:
        node = pending.pop()
        pending.extend(reversed(node.named_children))
        kind = KINDS.get(node.type)
        if not kind:
            continue
        if node.type == 'function_declarator':
            ancestor = node.parent
            duplicate = False
            while ancestor and ancestor.type not in {'translation_unit','namespace_definition','class_specifier','struct_specifier'}:
                if ancestor.type == 'function_definition':
                    duplicate = True
                    break
                ancestor = ancestor.parent
            if duplicate:
                continue
        name = declaration_name(node,raw)
        if not name:
            continue
        qualifiers = []
        ancestor = node.parent
        while ancestor:
            if ancestor.type in {'namespace_definition','class_specifier','struct_specifier','class_definition','class_declaration','struct_declaration','namespace_declaration','file_scoped_namespace_declaration'}:
                qualifier = ancestor.child_by_field_name('name')
                if qualifier:
                    qualifiers.append(node_text(qualifier,raw))
            ancestor = ancestor.parent
        body = node.child_by_field_name('body')
        end = body.start_byte if body else node.end_byte
        signature = ' '.join(raw[node.start_byte:end].decode('utf-8','replace').split())[:320]
        symbols.append({'name':name,'qualifiedName':'::'.join([*reversed(qualifiers),name]),'kind':kind,
                        'line':node.start_point.row+1,'endLine':node.end_point.row+1,'signature':signature})
    includes = re.findall(rb'^\s*#\s*include\s*"([^"\r\n]+)"',raw,re.M) if language == 'cpp' else []
    return {'symbols':symbols,'includes':[s.decode('utf-8','replace') for s in includes], 'parseErrors':tree.root_node.has_error}


def snapshot(path):
    with path.open('rb') as stream:
        raw = stream.read(4 * 1024 * 1024 + 1)
    if len(raw) > 4 * 1024 * 1024:
        raise QueryError('File grew beyond snapshot limit')
    return path,raw,hashlib.sha256(raw).hexdigest()


def index(root, scope, module, deadline):
    if scope not in {'source','tools','tests'}:
        raise QueryError('Syntax indexing supports source, tools and tests scopes')
    parsers,versions = backend()
    files = source_files(root,scope_paths(root,scope,module),deadline)
    key = hashlib.sha256((scope+'\0'+(module or '')).encode()).hexdigest()[:16]
    cache = safe_path(root,'build/code-intelligence/'+key+'.json')
    implementation = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    previous = {}
    try:
        previous = json.loads(cache.read_text(encoding='utf-8'))
        if previous.get('schemaVersion') != 1 or previous.get('versions') != versions or previous.get('implementation') != implementation:
            previous = {}
    except (OSError,ValueError):
        pass
    old = previous.get('files',{})
    current = {}
    parsed = 0
    unsupported = 0
    supported = [p for p in files if p.suffix.lower() in EXTENSIONS]
    unsupported = len(files)-len(supported)
    # Rehash every query, even for equal-size edits with restored timestamps.
    # Parallel bounded snapshots avoid serial Windows open/antivirus latency;
    # parser use stays on this thread, and at most 24 file snapshots are queued.
    with ThreadPoolExecutor(max_workers=8) as executor:
        for offset in range(0,len(supported),24):
            if time.monotonic() >= deadline:
                raise QueryError('Query deadline exceeded')
            try:
                snapshots = executor.map(snapshot,supported[offset:offset+24],timeout=max(0.001,deadline-time.monotonic()))
                for path,raw,digest in snapshots:
                    if time.monotonic() >= deadline:
                        raise QueryError('Query deadline exceeded')
                    language = EXTENSIONS[path.suffix.lower()]
                    name = path.relative_to(root).as_posix()
                    if name in old and old[name].get('sha256') == digest:
                        current[name] = old[name]
                    else:
                        current[name] = dict(sha256=digest,**parse_file(parsers[language],raw,language))
                        parsed += 1
            except FutureTimeout as error:
                raise QueryError('Query deadline exceeded') from error
    fingerprint = hashlib.sha256(json.dumps({k:v['sha256'] for k,v in sorted(current.items())},sort_keys=True).encode()).hexdigest()
    removed = len(set(old)-set(current))
    if parsed or removed or not previous:
        cache.parent.mkdir(parents=True,exist_ok=True)
        temporary = cache.with_name(cache.name+'.'+uuid.uuid4().hex+'.tmp')
        try:
            temporary.write_text(json.dumps({'schemaVersion':1,'versions':versions,'implementation':implementation,'files':current},ensure_ascii=False),encoding='utf-8')
            os.replace(temporary,cache)
        finally:
            if temporary.exists():temporary.unlink()
    metadata_row = {'fingerprint':fingerprint,'cacheHit':bool(previous) and not parsed and not removed,
                    'parsedFiles':parsed,'removedFiles':removed,'indexedFiles':len(current),'unsupportedFiles':unsupported,
                    'parseErrorFiles':sum(f['parseErrors'] for f in current.values()),
                    'backend':'tree-sitter','versions':versions,'precision':'syntax','preprocessorEvaluated':False}
    return current,metadata_row


def query(root, term, scope, module, limit, deadline, operation='symbols', depth=1):
    if not term or not 0 <= depth <= 2:
        raise QueryError('Use a nonempty query and map depth in 0..2')
    files,info = index(root,scope,module,deadline)
    matches = []
    for path,row in sorted(files.items()):
        for symbol in row['symbols']:
            if term.casefold() in symbol['qualifiedName'].casefold():
                matches.append(dict(path=path,sha256=row['sha256'],syntaxErrors=row['parseErrors'],shaderApproximation=Path(path).suffix.lower() in {'.frag','.vert','.comp','.glsl','.geom'},**symbol))
    matches.sort(key=lambda r:(r['name'].casefold()!=term.casefold(),r['path'],r['line']))
    payload = base_payload(operation,scope)
    payload['index'] = info
    if operation == 'symbols':
        payload.update(results=matches[:limit],truncated=len(matches)>limit)
        return payload
    seeds = list(dict.fromkeys(r['path'] for r in matches))
    if not seeds:
        seeds = [p for p in files if term.casefold() in p.casefold()]
    links = {p:set() for p in files}
    for path,row in files.items():
        for include in row['includes']:
            for base in (root/Path(path).parent,root/'src',root):
                candidate = Path(os.path.abspath(base/include))
                if root not in candidate.parents:
                    continue
                target = candidate.relative_to(root).as_posix()
                if target in files:
                    links[path].add(target)
                    links[target].add(path)
                    break
    selected = list(seeds[:limit])
    frontier = selected[:]
    for _ in range(depth):
        following = sorted({p for current in frontier for p in links[current]}-set(selected))
        selected.extend(following)
        frontier = following
        if len(selected)>limit:
            break
    payload['truncated'] = len(selected)>limit or len(seeds)>limit
    for path in selected[:limit]:
        row = files[path]
        symbols = sorted(row['symbols'],key=lambda s:(term.casefold() not in s['qualifiedName'].casefold(),s['line']))
        payload['results'].append({'path':path,'sha256':row['sha256'],'relation':'definition' if path in seeds else 'include-neighbor',
                                   'syntaxErrors':row['parseErrors'],'symbols':symbols[:4],'symbolsTruncated':len(symbols)>4})
    return payload
