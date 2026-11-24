"""Measure the complete editor CPU budget while browsing 10000 assets."""
import argparse
import json
import os
import shutil
import subprocess
import time
import uuid
from pathlib import Path


def run(executable, root):
    if root.exists():
        root = root / ('run-' + uuid.uuid4().hex[:8])
    root.mkdir(parents=True)
    source = Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector'
    shutil.copytree(source, root / 'game', ignore=shutil.ignore_patterns('.azure'))
    assets = root / 'game/assets/large'
    assets.mkdir()
    for i in range(10000):
        (assets / f'asset-{i:05d}.txt').write_text(str(i), encoding='utf-8')
    environment = os.environ.copy()
    environment['AZURERENDER_EDITOR_CONFIG'] = str(root / 'config')
    environment.pop('AZURERENDER_EDITOR_UI_ACTIONS', None)
    arguments = [str(executable), '--editor-project', str(root / 'game/project.azureproject'),
                 '--editor-close-policy', 'save', '--width', '1280', '--height', '720',
                 '--smoke-frames', '2100', '--fixed-frame-step', '--gpu-timing',
                 '--gpu-timing-output', str(root / 'timing.json'),
                 '--runtime-report', str(root / 'runtime.json')]
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    started = time.monotonic()
    with (root / 'stdout.log').open('w', encoding='utf-8') as stdout, (root / 'stderr.log').open('w', encoding='utf-8') as stderr:
        result = subprocess.run(arguments, cwd=root, env=environment, startupinfo=startup,
                                stdout=stdout, stderr=stderr, timeout=300)
    assert result.returncode == 0, (root / 'stderr.log').read_text()
    timing = json.loads((root / 'timing.json').read_text())
    cpu = timing['cpuFrame']
    offset = cpu['sampleOffsetFrame']
    samples = cpu['workSamplesMs'][max(0, 300 - offset):]
    assert len(samples) == 1800, (offset, len(samples), cpu['completedFrames'])
    ordered = sorted(samples)
    p95 = ordered[int(len(ordered) * .95)]
    workspace = json.loads((root / 'runtime.json').read_text())['editorWorkspace']
    assert workspace['contentNavigation']['statistics']['catalogBuilds'] == 1
    assert workspace['contentNavigation']['statistics']['directoryEnumerations'] == 1
    assert 0 < len(workspace['visibleAssets']) < 100
    assert not workspace['uiErrors'] and not workspace['feedback']
    summary = dict(stage='U10', assets=10000, warmupFrames=300, sampleFrames=1800,
                   editorCpuP95Ms=p95, editorCpuBudgetMs=10, passed=p95 <= 10,
                   renderedRows=len(workspace['visibleAssets']),
                   catalog=workspace['contentNavigation']['statistics'],
                   device=timing['gpu'],
                   processSeconds=time.monotonic() - started)
    (root / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary), flush=True)
    assert p95 <= 10, summary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.executable.resolve(), args.output.resolve())
