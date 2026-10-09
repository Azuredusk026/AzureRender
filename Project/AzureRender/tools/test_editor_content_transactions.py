"""Verify file and texture operations through production editor input."""
import argparse
import json
import shutil
import uuid
from pathlib import Path
from PIL import Image
from test_editor_workspace import launch


def run(executable, root):
    if root.exists():
        root = root / ('run-' + uuid.uuid4().hex[:8])
    root.mkdir(parents=True)
    source = Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector'
    shutil.copytree(source, root / 'game', ignore=shutil.ignore_patterns('.azure'))
    image_id = '11111111-1111-4111-8111-111111111111'
    image = root / 'game/assets/settings.png'
    Image.new('RGBA', (8, 8), (120, 140, 190, 255)).save(image)
    metadata = dict(schemaVersion=1, id=image_id, settings={}, dependencies=[])
    Path(str(image) + '.azmeta').write_text(json.dumps(metadata), encoding='utf-8')
    second_id = '22222222-2222-4222-8222-222222222222'
    other = image.parent / 'settings-b.png'
    Image.new('RGBA', (8, 8), (128, 128, 255, 255)).save(other)
    other_meta = dict(schemaVersion=1, id=second_id, dependencies=[],
                      settings=dict(texture=dict(usage='normal', colorSpace='linear', wrapV='clamp')))
    Path(str(other) + '.azmeta').write_text(json.dumps(other_meta), encoding='utf-8')
    actions = [dict(frame=25, action='click', target='assets.type'),
               dict(frame=30, action='click', target='type.Images'),
               dict(frame=33, action='click', target='asset.' + image_id),
               dict(frame=35, action='click', target='asset.' + second_id, ctrl=True),
               dict(frame=38, action='click', target='asset.' + image_id, button='right'),
               dict(frame=44, action='click', target='asset.texture-settings'),
               dict(frame=50, action='click', target='texture.enable.filter'),
               dict(frame=55, action='click', target='texture.filter'),
               dict(frame=60, action='click', target='texture.option.filter.nearest'),
               dict(frame=65, action='click', target='texture.plan'),
               dict(frame=72, action='click', target='content.apply')]
    result = launch(executable, root, 'texture', (1920, 1080), 1, actions)
    saved = json.loads(Path(str(image) + '.azmeta').read_text(encoding='utf-8'))
    assert saved['settings']['texture']['filter'] == 'nearest', saved
    other_saved = json.loads(Path(str(other) + '.azmeta').read_text(encoding='utf-8'))
    assert other_saved['settings']['texture']['filter'] == 'nearest', other_saved
    assert other_saved['settings']['texture']['usage'] == 'normal'
    assert other_saved['settings']['texture']['wrapV'] == 'clamp'
    assert result['feedback'] == [], result['feedback']
    layout = launch(executable, root, 'layout', (1920, 1080), 1, actions[:9])
    assert layout['widgets']['texture.dialog'][2] >= 558, layout['widgets']['texture.dialog']
    assert result['selection'] == ['observer'], result['selection']
    second = launch(executable, root, 'reopen', (1920, 1080), 1, capture=False)
    assert second['feedback'] == [], second['feedback']
    rename = [dict(frame=25, action='click', target='assets.type'),
              dict(frame=30, action='click', target='type.Images'),
              dict(frame=38, action='click', target='asset.' + image_id, button='right'),
              dict(frame=44, action='click', target='asset.rename'),
              dict(frame=50, action='click', target='content.value'),
              dict(frame=53, action='key', key='A', down=True, ctrl=True),
              dict(frame=54, action='key', key='A', down=False, ctrl=True),
              dict(frame=55, action='key', key='A', down=False, ctrl=False),
              dict(frame=57, action='text', text='edited.png'),
              dict(frame=63, action='click', target='content.plan'),
              dict(frame=70, action='click', target='content.apply')]
    renamed = launch(executable, root, 'rename', (1920, 1080), 1, rename, capture=False)
    assert not image.exists() and (image.parent / 'edited.png').exists()
    assert renamed['feedback'] == [], renamed['feedback']
    assert json.loads((image.parent / 'edited.png.azmeta').read_text(encoding='utf-8'))['id'] == image_id
    undo = launch(executable, root, 'undo', (1920, 1080), 1, [
        dict(frame=25, action='click', target='assets.import-create'),
        dict(frame=32, action='click', target='content.undo')], capture=False)
    assert image.exists() and not (image.parent / 'edited.png').exists()
    assert undo['feedback'] == [], undo['feedback']
    diagnostics = launch(executable, root, 'diagnostics', (1920, 1080), 1, [
        dict(frame=25, action='click', target='assets.type'),
        dict(frame=30, action='click', target='type.Images'),
        dict(frame=38, action='click', target='asset.' + image_id, button='right'),
        dict(frame=44, action='click', target='asset.inspect'),
        dict(frame=55, action='click', target='content.close-dependencies')], capture=False)
    assert diagnostics['feedback'] == [] and not diagnostics['dirty'], diagnostics['feedback']
    summary = dict(passed=True, stage='G12', textureInput=True, impactPreview=True,
                   saveReopen=True, selectedBatchFields=True, renameInput=True, durableUndoInput=True, readOnlyDiagnostics=True,
                   physicalKeyboardMouse='not performed')
    (root / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    run(args.executable.resolve(), args.output.resolve())
