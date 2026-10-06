"""Check first-party CMake target dependencies and transitive local includes."""
import argparse
import json
from pathlib import Path
import re

ALLOWED = {
    'AzureValidation': {'AzureFoundation', 'AzureReflection', 'AzureRenderCore', 'AzureRuntime'},
    'AzureFoundation': set(),
    'AzurePlatform': {'AzureFoundation'},
    'AzureRenderCore': {'AzureFoundation'},
    'AzureReflection': {'AzureFoundation'},
    'AzureRuntime': {'AzureFoundation', 'AzureReflection', 'AzureRenderCore'},
    'AzureGameplay': {'AzureFoundation', 'AzureRenderCore', 'AzureReflection', 'AzureRuntime'},
    'AzureProjectRuntime': {'AzureFoundation', 'AzureRenderCore', 'AzureReflection', 'AzureRuntime', 'AzureGameplay'},
    'AzureEditor': {'AzureFoundation', 'AzurePlatform', 'AzureRenderCore', 'AzureReflection', 'AzureRuntime', 'AzureGameplay', 'AzureProjectRuntime'},
    'AzurePlayerHost': {'AzureFoundation', 'AzurePlatform', 'AzureRenderCore', 'AzureReflection', 'AzureRuntime', 'AzureGameplay', 'AzureProjectRuntime', 'AzureValidation'},
    'AzureRenderHost': {'AzureFoundation', 'AzurePlatform', 'AzureRenderCore', 'AzureReflection', 'AzureRuntime', 'AzureEditor', 'AzureGameplay', 'AzureProjectRuntime', 'AzureValidation'},
}

def effective_includes(text, definitions):
    """Honor known build guards; conservatively inspect both unknown branches."""
    branches = []
    for line in text.splitlines():
        directive = re.match(r'^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)',line)
        if directive:
            kind, expression = directive[1], directive[2].strip()
            if kind == 'endif':
                if branches:
                    branches.pop()
            elif kind == 'else':
                if branches and branches[-1] is not None:
                    branches[-1] = not branches[-1]
            else:
                result = None
                match = re.fullmatch(r'(!)?\s*([A-Za-z_]\w*)',expression)
                if match and match[2] in definitions:
                    value = definitions[match[2]]
                    if kind in {'ifdef','ifndef'}:
                        result = kind == 'ifdef'
                    elif value in {'0','1'}:
                        result = (value != '0') != bool(match[1])
                if kind == 'elif':
                    if branches:
                        branches[-1] = None  # Inspect alternative expressions conservatively.
                else:
                    branches.append(result)
            continue
        match = re.match(r'^\s*#\s*include\s*"([^"\n]+)"',line)
        if match and False not in branches:
            yield match[1]

def owner(path):
    parts = path.parts
    if 'src' not in parts:
        return None
    tail = parts[parts.index('src') + 1:]
    if not tail:
        return None
    area = tail[0]
    if area == 'validation':
        return 'AzureValidation'
    if area == 'gameplay':
        return 'AzureGameplay'
    if area == 'app' and path.name.startswith('ProjectRuntimeAssembly'):
        return 'AzureProjectRuntime'
    if area == 'ecs' or (area == 'reflection' and path.name == 'Annotations.hpp'):
        return None  # Shared value contracts contain no runtime implementation.
    if area == 'extensions':
        if path.name in {'ExtensionDescriptor.hpp', 'IEditorPanel.hpp', 'SceneType.hpp'}:
            return None
        return 'AzureRenderCore'
    if area == 'diagnostics':
        return 'AzureRenderCore' if path.name.startswith('GpuCapability') else 'AzureFoundation'
    return {'foundation':'AzureFoundation', 'resources':'AzureFoundation', 'platform':'AzurePlatform', 'reflection':'AzureReflection',
            'runtime':'AzureRuntime', 'editor':'AzureEditor', 'assets':'AzureRenderCore',
            'render':'AzureRenderCore', 'rhi':'AzureRenderCore', 'scene':'AzureRenderCore',
            'scenes':'AzureRenderCore'}.get(area)

def check(source, build):
    reply = build / '.cmake/api/v1/reply'
    models = sorted(reply.glob('codemodel-v2*.json'), key=lambda p:p.stat().st_mtime_ns)
    if not models:
        raise ValueError('CMake codemodel missing; configure after creating the File API query')
    model = json.loads(models[-1].read_text(encoding='utf-8'))
    violations = []
    source_targets = {}
    targets = {}
    definitions = {}
    for configuration in model['configurations']:
        for reference in configuration['targets']:
            data = json.loads((reply / reference['jsonFile']).read_text(encoding='utf-8'))
            targets[reference['id']] = data
            definitions[data['name']] = {record['define'].split('=',1)[0]:record['define'].split('=',1)[1] if '=' in record['define'] else '1'
                                         for group in data.get('compileGroups',[]) for record in group.get('defines',[])}
            if data['name'] in ALLOWED:
                for record in data.get('sources', []):
                    path = Path(record['path'])
                    if not path.is_absolute():
                        path = (build if record.get('isGenerated') else source) / path
                    if path.suffix in {'.cpp', '.hpp', '.h'}:
                        source_targets.setdefault(path.resolve(), set()).add(data['name'])
    for data in targets.values():
        name = data['name']
        if name not in ALLOWED:
            continue
        for dependency in data.get('dependencies', []):
            dest = targets.get(dependency['id'], {}).get('name')
            if dest in ALLOWED and dest != name and dest not in ALLOWED[name]:
                violations.append({'kind':'dependency','from':name,'to':dest})
    files = set(source_targets)
    for path in (source/'src').rglob('*'):
        if path.suffix in {'.cpp', '.hpp', '.h'}:
            files.add(path.resolve())
    include_count = 0
    for entry in sorted(files):
        users = source_targets.get(entry) or ({owner(entry)} if owner(entry) else set())
        for user in users:
            seen = set()
            pending = [entry]
            while pending:
                current = pending.pop()
                if current in seen or not current.is_file():
                    continue
                seen.add(current)
                text = current.read_text(encoding='utf-8-sig')
                for include in effective_includes(text,definitions.get(user,{})):
                    candidates = [current.parent/include, source/'src'/include, build/'generated'/include]
                    target = next((p.resolve() for p in candidates if p.is_file()), None)
                    if not target:
                        continue  # System and third-party headers belong to their own targets.
                    include_count += 1
                    owners = source_targets.get(target) or ({owner(target)} if owner(target) else set())
                    for dest in owners:
                        if dest in ALLOWED and dest != user and dest not in ALLOWED[user]:
                            violations.append({'kind':'include','from':user,'to':dest,'entry':str(entry.relative_to(source)) if entry.is_relative_to(source) else str(entry),
                                               'file':str(current),'include':include})
                    pending.append(target)
    unique = {json.dumps(v, sort_keys=True):v for v in violations}
    return {'status':'failed' if unique else 'passed', 'targets':len([t for t in targets.values() if t['name'] in ALLOWED]),
            'includes':include_count, 'violations':list(unique.values())}

def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        report = check(args.source.resolve(), args.build_dir.resolve())
    except (ValueError, KeyError, OSError) as error:
        report = {'status':'failed', 'error':str(error)}
    serialized = json.dumps(report, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(serialized + '\n', encoding='utf-8')
    print(serialized)
    return 0 if report['status'] == 'passed' else 1

if __name__ == '__main__':
    raise SystemExit(main())
