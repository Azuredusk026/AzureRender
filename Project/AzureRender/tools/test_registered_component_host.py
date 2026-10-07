"""Verify an independently registered component in the Vulkan editor host."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--project', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure_registered_host_') as folder:
        root = Path(folder)
        shutil.copytree(args.project.resolve().parent, root / 'project')
        actions = [
            {'frame': 1, 'command': 'select', 'id': 'hero'},
            {'frame': 2, 'command': 'component-add', 'type': 'tool.asset-note'},
            {'frame': 3, 'command': 'component-field', 'type': 'tool.asset-note', 'field': 'text', 'value': 'inspected'},
            {'frame': 4, 'command': 'component-field', 'type': 'tool.asset-note', 'field': 'sampleCount', 'value': 9, 'expectError': True},
            {'frame': 5, 'command': 'save'},
            {'frame': 6, 'command': 'reload'},
            {'frame': 7, 'command': 'select', 'id': 'hero'},
            {'frame': 9, 'command': 'play'},
            {'frame': 14, 'command': 'stop'},
        ]
        task = root / 'actions.json'
        task.write_text(json.dumps(actions), encoding='utf-8')
        report = root / 'report.json'
        result = subprocess.run([
            str(args.executable.resolve()), '--editor-close-policy', 'save', '--editor-project', str(root / 'project/project.azureproject'),
            '--editor-actions', str(task), '--runtime-report', str(report),
            '--fixed-frame-step', '--smoke-frames', '18'],
            cwd=root, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=90)
        assert result.returncode == 0, result.stdout + result.stderr
        data = json.loads(report.read_text(encoding='utf-8'))
        assert len(data['editorActions']) == len(actions), data
        assert all(item['passed'] for item in data['editorActions']), data
        assert data['uiDrawCalls'] > 0 and data['editorPlaySteps'] > 0, data
        assert data['editStateRestored'] and not data['editorPlaying'], data
        assert 'Validation Error' not in result.stdout + result.stderr
        level = json.loads((root / 'project/assets/courtyard.azurelevel').read_text(encoding='utf-8'))
        hero = next(node for node in level['nodes'] if node['id'] == 'hero')
        assert hero['components']['tool.asset-note']['data'] == {'text': 'inspected', 'sampleCount': 3}, hero
        print(json.dumps(data, ensure_ascii=False))


if __name__ == '__main__':
    main()
