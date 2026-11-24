"""Retain the final two-project layout and visible-content DPI matrix."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import time

ROOT = Path.cwd().resolve()
sys.path.insert(0, str(ROOT / 'tools'))
from create_playable_project import create
from test_editor_workspace import launch


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve() / ('run-' + str(time.time_ns()))
    summaries = []
    for workflow in ('exploration', 'inspector'):
        folder = output / workflow
        folder.mkdir(parents=True)
        if workflow == 'exploration':
            create(folder / 'game')
        else:
            shutil.copytree(ROOT / 'assets_public/scene_inspector', folder / 'game',
                            ignore=shutil.ignore_patterns('.azure'))
        descriptor = json.loads((folder / 'game/project.azureproject').read_text(encoding='utf-8'))
        level = folder / 'game/assets' / descriptor['startupScene'].removeprefix('assets:/')
        authored = level.read_bytes()
        for dpi in (1, 1.5, 2):
            for width, height in ((1920, 1080), (1280, 720)):
                case = f'{width}x{height}-{dpi}'
                state = launch(args.executable.resolve(), folder, case, (width, height), dpi,
                               close_policy='discard')
                panel = state['panels']['assets']['rect']
                first = state['visibleAssets'][0]['id']
                rect = state['widgets']['asset.' + first]
                assert panel[1] <= rect[1] and rect[1] + rect[3] <= panel[1] + panel[3], (workflow, case)
                assert all(p['docked'] for key, p in state['panels'].items() if key != 'settings')
                assert level.read_bytes() == authored
                (folder / case / 'workspace.json').write_text(json.dumps(state, indent=2), encoding='utf-8')
                summaries.append(dict(workflow=workflow, case=case, passed=True,
                    imageExtent=[state['image']['width'], state['image']['height']],
                    firstAssetVisible=True, authoredContentPreserved=True,
                    screenshot=f'{workflow}/{case}/capture/frame_000015.png'))
    result = dict(passed=True, cases=summaries,
                  executableSha256=hashlib.sha256(args.executable.read_bytes()).hexdigest())
    (output / 'summary.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
