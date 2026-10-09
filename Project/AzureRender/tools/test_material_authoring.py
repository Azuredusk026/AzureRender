"""Exercise material production forms, model slots and Player rendering."""
import argparse
import json
import os
import shutil
import subprocess
import uuid
from pathlib import Path
from PIL import Image, ImageChops, ImageStat
from test_editor_workspace import launch


def run(executable, player, root):
    if root.exists():
        root = root / ('run-' + uuid.uuid4().hex[:8])
    root.mkdir(parents=True)
    source = Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector'
    shutil.copytree(source, root / 'game', ignore=shutil.ignore_patterns('.azure'))
    model = root / 'game/assets/cube.gltf'
    mesh = json.loads(model.read_text(encoding='utf-8'))
    second = json.loads(json.dumps(mesh['materials'][0]))
    second['name'] = 'second-surface'
    mesh['materials'].append(second)
    mesh['meshes'][0]['primitives'][1]['material'] = 1
    model.write_text(json.dumps(mesh), encoding='utf-8')
    baseline = launch(executable, root, 'baseline', (1920, 1080), 1, [dict(frame=25, action='click', target='node.asset-a')])
    create_actions = [dict(frame=25, action='click', target='assets.import-create'),
                      dict(frame=32, action='click', target='assets.create-material'),
                      dict(frame=40, action='click', target='material.create.azure.pbr'),
                      dict(frame=50, action='click', target='material.field.metallic', offset=[-90, 0])]
    made = launch(executable, root, 'create-pbr', (1920, 1080), 1, create_actions)
    first = root / 'game/assets/Material.azurematerial'
    document = json.loads(first.read_text(encoding='utf-8'))
    assert document['typeId'] == 'azure.pbr' and 0 < document['parameters']['metallic'] < 1, document
    assert not made['dirty'] and made['feedback'] == []
    first_id = json.loads(Path(str(first) + '.azmeta').read_text(encoding='utf-8'))['id']
    style_actions = create_actions[:2] + [
        dict(frame=40, action='click', target='material.create-name'),
        dict(frame=43, action='key', key='A', down=True, ctrl=True),
        dict(frame=44, action='key', key='A', down=False, ctrl=True),
        dict(frame=45, action='key', key='A', down=False, ctrl=False),
        dict(frame=47, action='text', text='Style.azurematerial'),
        dict(frame=54, action='click', target='material.create.azure.stylized'),
        dict(frame=62, action='click', target='material.field.toonAmount', offset=[-90, 0])]
    styled = launch(executable, root, 'create-style', (1920, 1080), 1, style_actions)
    style = root / 'game/assets/Style.azurematerial'
    assert json.loads(style.read_text(encoding='utf-8'))['typeId'] == 'azure.stylized'
    assert styled['feedback'] == [] and not styled['dirty']
    style_id = json.loads(Path(str(style) + '.azmeta').read_text(encoding='utf-8'))['id']
    assign = [dict(frame=25, action='click', target='node.asset-a'),
              dict(frame=31, action='click', target='material.slots-header'),
              dict(frame=38, action='click', target='material.slot.name:stone'),
              dict(frame=43, action='click', target='material.assign.name:stone.' + first_id),
              dict(frame=50, action='click', target='material.slot.name:second-surface'),
              dict(frame=55, action='click', target='material.assign.name:second-surface.' + style_id),
              dict(frame=63, action='click', target='save')]
    assigned = launch(executable, root, 'assign-slots', (1920, 1080), 1, assign)
    assert assigned['feedback'] == [] and not assigned['dirty'], assigned['feedback']
    level = root / 'game/assets/inspection.azurelevel'
    scene = json.loads(level.read_text(encoding='utf-8'))
    node = next(item for item in scene['nodes'] if item['id'] == 'asset-a')
    assert node['materialSlots'] == {'name:stone': first_id, 'name:second-surface': style_id}
    image_rect = baseline['image']
    box = tuple(round(image_rect[k]) for k in ('x', 'y'))
    box += (box[0] + round(image_rect['width']), box[1] + round(image_rect['height']))
    before = Image.open(sorted((root / 'baseline/capture').glob('frame_*.png'))[-1]).convert('RGB').crop(box)
    after = Image.open(sorted((root / 'assign-slots/capture').glob('frame_*.png'))[-1]).convert('RGB').crop(box)
    difference = max(ImageStat.Stat(ImageChops.difference(before, after)).mean)
    assert difference > 0.2, ('Slot edits must change actual viewport pixels', difference)
    for name, base in [('source-player', root / 'game'), ('relocated-player', root / 'moved-game')]:
        if name == 'relocated-player':
            shutil.copytree(root / 'game', base, ignore=shutil.ignore_patterns('.azure'))
        folder = root / name
        folder.mkdir()
        report = folder / 'runtime.json'
        info = subprocess.STARTUPINFO()
        info.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        info.wShowWindow = 0
        result = subprocess.run([str(player), '--project', str(base / 'project.azureproject'),
                                 '--runtime-report', str(report), '--smoke-frames', '36', '--fixed-frame-step'],
                                cwd=folder, capture_output=True, encoding='utf-8', errors='replace', timeout=180, startupinfo=info)
        (folder / 'stdout.log').write_text(result.stdout, encoding='utf-8')
        (folder / 'stderr.log').write_text(result.stderr, encoding='utf-8')
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'VUID-' not in result.stderr and 'Validation Error' not in result.stderr
        assert 'Allocator after unload: buffers=0 images=0' in result.stdout
    summary = dict(stage='U11', passed=True, types=['azure.pbr', 'azure.stylized'], actualSubmeshSlots=2,
                   viewportMeanDifference=difference, playerRuns=2, physicalKeyboardMouse='not performed',
                   releasePackaging='separate acceptance receipt')
    (root / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--player', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    run(args.executable.resolve(), args.player.resolve(), args.output.resolve())
