"""Ensure a production publish receipt retains its own target and log."""
import argparse
import json
from pathlib import Path
import shutil
import time
import uuid

from editor_test_host import EditorHost


def fill(host, target, value):
    host.click(target)
    for down in (True, False):
        host.call('input', event=dict(action='key', key='A', ctrl=down, down=down))
        host.frames()
    host.call('input', event=dict(action='text', text=str(value)))
    host.frames()


def run(executable, install, output):
    root = output / ('run-' + uuid.uuid4().hex[:8])
    project = root / 'source'
    source = Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector'
    shutil.copytree(source, project, ignore=shutil.ignore_patterns('.azure'))
    published = root / '首次发布'
    next_target = root / '下次发布'
    with EditorHost(executable, project / 'project.azureproject', root / 'editor') as host:
        host.click('build')
        fill(host, 'install.path', install)
        fill(host, 'output.path', published)
        x, y, width, height = host.workspace()['widgets']['project.build']
        assert 0 <= x < x + width <= 1920 and 0 <= y < y + height <= 1080, (x, y, width, height)
        host.click('project.build')
        deadline = time.monotonic() + 120
        while time.monotonic() < deadline:
            host.frames()
            receipt = host.workspace()['build']
            if not receipt['running'] and receipt['message']:
                break
        else:
            raise AssertionError('Publish did not finish')
        assert receipt['passed'], receipt
        assert published.is_dir()
        assert Path(receipt['resultOutput']) == published
        assert Path(receipt['resultLog']) == project / '.azure/game-build.log'
        assert Path(receipt['resultLog']).is_file()
        fill(host, 'output.path', next_target)
        workspace = host.workspace()
        assert Path(workspace['build']['output']) == next_target
        assert Path(workspace['build']['resultOutput']) == published
        assert 'project.build.reveal' in workspace['widgets']
        assert not next_target.exists()
    result = dict(passed=True, originalOutput=str(published),
                  formAfterPublish=str(next_target), log=receipt['resultLog'])
    (root / 'summary.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--install', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.executable.resolve(), args.install.resolve(), args.output.resolve())
