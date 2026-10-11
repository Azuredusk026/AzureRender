"""Verify native output filenames and UTF-8 metadata in both runtime hosts."""
import argparse
import json
import shutil
import subprocess
from pathlib import Path
import tempfile


def run(executable, source, root):
    root.mkdir(parents=True, exist_ok=True)
    model = root / '图 🎨 模型.GLB'
    shutil.copy2(source, model)
    project = root / 'game'
    creator = executable.parent / 'AzurePlayer.exe'
    created = subprocess.run([str(creator), '--create-project', str(project)], capture_output=True, encoding='utf-8', errors='replace', timeout=30)
    assert created.returncode == 0, created.stdout + created.stderr
    config = json.loads((project / 'project.azureproject').read_text(encoding='utf-8'))
    config['startupScene'] = 'assets:/unicode.azurelevel'
    (project / 'project.azureproject').write_text(json.dumps(config), encoding='utf-8')
    shutil.copy2(model, project / 'assets' / model.name)
    level = dict(schemaVersion=1, id='unicode', sceneType='sample', resources=[dict(id='mesh', asset='assets:/' + model.name)], nodes=[dict(id='model', resourceId='mesh', components={})])
    (project / 'assets/unicode.azurelevel').write_text(json.dumps(level, ensure_ascii=False), encoding='utf-8')
    if executable.stem == 'AzureRender':
        scene = root / '创建 🎨.azscene'
        generated = subprocess.run([str(executable), '--asset', str(model), '--create-scene', str(scene)], capture_output=True, encoding='utf-8', errors='replace', timeout=30)
        assert generated.returncode == 0 and scene.is_file(), generated.stdout + generated.stderr
    capture = root / '截图 🎨'
    report = root / '运行 🎨.json'
    result = subprocess.run([str(executable), '--editor-project' if executable.stem == 'AzureRender' else '--project', str(project / 'project.azureproject'), '--asset', str(model), '--capture-dir',
                             str(capture), '--capture-frames', '2', '--capture-fps', '4',
                             '--runtime-report', str(report)], cwd=root, capture_output=True,
                            encoding='utf-8', errors='replace', timeout=90)
    (root / 'stdout.log').write_text(result.stdout, encoding='utf-8')
    (root / 'stderr.log').write_text(result.stderr, encoding='utf-8')
    assert result.returncode == 0, result.stdout + result.stderr
    assert report.is_file(), 'Native Unicode runtime report must be published'
    json.loads(report.read_text(encoding='utf-8'))
    assert len(list(capture.glob('frame_*.png'))) == 2
    manifest = json.loads((capture / 'capture_manifest.json').read_text(encoding='utf-8'))
    expected = project / 'assets' / model.name if executable.stem == 'AzureRender' else model
    assert Path(manifest['asset']).resolve() == expected.resolve(), manifest['asset']
    assert 'VUID-' not in result.stderr and 'Validation Error' not in result.stderr
    assert 'Allocator after unload: buffers=0 images=0' in result.stdout
    print('Unicode output filenames and metadata passed:', executable.name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure-unicode-') as temporary:
        run(args.executable.resolve(), args.source.resolve(), args.output.resolve() if args.output else Path(temporary))
