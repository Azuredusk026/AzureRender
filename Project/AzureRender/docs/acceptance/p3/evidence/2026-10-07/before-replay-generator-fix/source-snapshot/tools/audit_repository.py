"""Report tracked local state, broken active links and ignore boundaries."""
import argparse
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote, urlsplit


def git(root, *args):
    return subprocess.run(['git', '-C', str(root), *args], capture_output=True, encoding='utf-8', check=False)


def audit(root):
    root = root.resolve()
    listed = git(root, 'ls-files', '-z')
    if listed.returncode:
        raise ValueError('Source must be a Git repository: ' + listed.stderr)
    tracked = listed.stdout.split('\0')
    state = sorted(p for p in tracked if p.startswith(('.codemaker/', '.claude/')) or p == 'opencode.json')
    engine = root / 'Project/AzureRender'
    documents = [root / 'README.md', engine / 'README.md']
    for directory in ('docs', 'docs/plans', 'docs/runtime', 'docs/tutorials'):
        documents.extend((engine / directory).glob('*.md'))
    broken = []
    for document in sorted(set(documents)):
        if not document.is_file():
            continue
        text = re.sub(r'```.*?```', '', document.read_text(encoding='utf-8'), flags=re.S)
        for match in re.finditer(r'\[[^\]]*\]\((?:<([^>]+)>|([^\s)]+)(?:\s+"[^"]*")?)\)', text):
            target = match.group(1) or match.group(2)
            parsed = urlsplit(target)
            if parsed.scheme or not parsed.path:
                continue
            path = document.parent / unquote(parsed.path)
            if not path.exists():
                broken.append(dict(document=document.relative_to(root).as_posix(), target=target))
    expected = {
        'Project/AzureRender/build/hygiene-probe.txt': True,
        '.codemaker/codemap/hygiene-probe.log': True,
        '.env': True,
        'Project/AzureRender/src/main.cpp': False,
        'Project/AzureRender/assets_public/hygiene-probe.gltf': False,
    }
    probes = []
    for path, ignored in expected.items():
        actual = git(root, 'check-ignore', '--no-index', '-q', path)
        if actual.returncode not in (0, 1):
            raise ValueError(actual.stderr)
        observed = actual.returncode == 0
        probes.append(dict(path=path, expectedIgnored=ignored, ignored=observed, passed=observed == ignored))
    passed = not state and not broken and all(p['passed'] for p in probes)
    return dict(schemaVersion=1, status='passed' if passed else 'failed',
        trackedToolState=state, danglingLinks=broken, ignoreProbes=probes)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
