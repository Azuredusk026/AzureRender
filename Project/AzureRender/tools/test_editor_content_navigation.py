"""Exercise directory navigation and explicit asset opening through real ImGui input."""
import argparse
import json
import shutil
import uuid
from pathlib import Path
from test_editor_workspace import launch


def run(executable, root):
    if root.exists():
        root = root / ('run-' + uuid.uuid4().hex[:8])
    root.mkdir(parents=True)
    source = Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector'
    shutil.copytree(source, root / 'game', ignore=shutil.ignore_patterns('.azure'))
    directory = root / 'game/assets/navigation/empty'
    directory.mkdir(parents=True)
    (directory.parent / 'guide.txt').write_text('Content navigation fixture', encoding='utf-8')
    initial = launch(executable, root, 'baseline', (1920, 1080), 1, capture=False)
    actions = [dict(frame=25, action='click', target='directory.assets:/navigation'),
               dict(frame=32, action='click', target='folder.assets:/navigation/empty'),
               dict(frame=34, action='click', target='folder.assets:/navigation/empty'),
               dict(frame=40, action='click', target='assets.back'),
               dict(frame=45, action='click', target='assets.forward')]
    navigation = launch(executable, root, 'navigation', (1920, 1080), 1, actions)
    assert navigation['contentNavigation']['mount'] == 'assets'
    assert navigation['contentNavigation']['path'] == 'navigation/empty'
    assert navigation['visibleAssets'] == []
    assert navigation['feedback'] == [] and not navigation['dirty']
    breadcrumb = launch(executable, root, 'breadcrumb', (1920, 1080), 1, actions + [
        dict(frame=52, action='click', target='assets.breadcrumb.navigation')], capture=False)
    assert breadcrumb['contentNavigation']['path'] == 'navigation'
    assert len(breadcrumb['visibleAssets']) == 1
    model = root / 'game/assets/cube.gltf'
    identity = json.loads(Path(str(model) + '.azmeta').read_text(encoding='utf-8'))['id']
    open_actions = [dict(frame=25, action='click', target='assets.search'),
                    dict(frame=28, action='text', text='cube.gltf'),
                    dict(frame=34, action='click', target='asset.' + identity),
                    dict(frame=36, action='click', target='asset.' + identity)]
    opened = launch(executable, root, 'open-model', (1920, 1080), 1, open_actions)
    assert opened['nodeCount'] == initial['nodeCount'] and not opened['dirty']
    assert opened['contentNavigation']['previewHandle'] > 0
    assert 'asset.close-preview' in opened['widgets'] and opened['feedback'] == []
    placed = launch(executable, root, 'place-model', (1920, 1080), 1, open_actions[:2] + [
        dict(frame=34, action='click', target='asset.' + identity, button='right'),
        dict(frame=40, action='click', target='asset.place'),
        dict(frame=45, action='click', target='save')], capture=False)
    assert placed['nodeCount'] == initial['nodeCount'] + 1 and placed['feedback'] == []
    assert not placed['dirty']
    generated = launch(executable, root, 'generate-primitive', (1920, 1080), 1, [
        dict(frame=25, action='click', target='assets.import-create'),
        dict(frame=32, action='click', target='assets.generate-primitive'),
        dict(frame=40, action='click', target='primitive.generate')], capture=False)
    primitive = root / 'game/assets/Primitive.glb'
    assert primitive.is_file() and generated['feedback'] == []
    provenance = json.loads(Path(str(primitive) + '.azmeta').read_text(encoding='utf-8'))
    assert provenance['generation']['generatorId'] == 'azure.primitive'
    assert generated['nodeCount'] == placed['nodeCount'] and not generated['dirty']
    light = launch(executable, root, 'create-light', (1920, 1080), 1, [
        dict(frame=25, action='click', target='assets.import-create'),
        dict(frame=32, action='click', target='assets.create-object'),
        dict(frame=40, action='click', target='preset.light'),
        dict(frame=46, action='click', target='save')], capture=False)
    assert light['nodeCount'] == placed['nodeCount'] + 1 and light['feedback'] == []
    descriptor = json.loads((root / 'game/project.azureproject').read_text(encoding='utf-8'))
    level = json.loads((root / 'game/assets' / descriptor['startupScene'].split(':/', 1)[1]).read_text(encoding='utf-8'))
    assert len(level['lights']) == 1
    summary = dict(passed=True, stage='U10', directoryTree=True, emptyFolder=True,
                   backForward=True, breadcrumb=True, doubleClickOpens=True,
                   explicitPlacement=True, primitiveCreation=True, lightPreset=True,
                   physicalKeyboardMouse='not performed')
    (root / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    run(args.executable.resolve(), args.output.resolve())
