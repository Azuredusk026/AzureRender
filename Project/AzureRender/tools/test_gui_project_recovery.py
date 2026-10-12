"""Check GUI recovery for missing, incompatible and relocated project documents."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
from test_gui_delivery_flow import NativeUI


def run(install, output):
    install = install.resolve(); output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='Azure Recovery 中文 '))
    original = root / '原位置'
    env = dict(os.environ, PATH=str(Path(os.environ['SystemRoot']) / 'System32'))
    env.pop('AZURERENDER_RESOURCE_ROOT', None)
    result = subprocess.run([str(install / 'bin/AzurePlayer.exe'), '--create-game', str(original)],
                            env=env, cwd=root, capture_output=True, timeout=60)
    assert result.returncode == 0, result.stderr
    document = json.loads((original / 'project.azureproject').read_text(encoding='utf-8'))
    moved = root / '移动后的位置'
    original.rename(moved)
    incompatible = moved / 'incompatible.azureproject'
    incompatible.write_text(json.dumps(dict(document, engine=dict(id='AzureRender', projectApiVersion=99))), encoding='utf-8')
    config = root / '中文用户配置'; config.mkdir()
    history = config / 'recent-projects.json'
    history.write_text(json.dumps(dict(schemaVersion=1, projects=[
        dict(id=document['id'], name=document['name'], path=str(original / 'project.azureproject')),
        dict(id='missing', name='失效记录', path=str(root / 'missing.azureproject')),
        dict(id='incompatible', name='版本不兼容', path=str(incompatible))]), ensure_ascii=False), encoding='utf-8')
    executable = install / 'bin/AzureRender.exe'
    with NativeUI(executable, config, output / 'recover', replay=True) as ui:
        state = ui.until(lambda s: len(s.get('recent', [])) == 3)
        assert all(not p['available'] for p in state['recent'])
        ui.snapshot('invalid-recents')
        ui.click('center.open.tab')
        ui.fill('center.open.path', str(root / 'missing.azureproject'))
        ui.click('center.open')
        ui.until(lambda s: s.get('diagnostic') and not s.get('busy'))
        ui.snapshot('missing-project')
        ui.fill('center.open.path', str(incompatible))
        ui.click('center.open')
        ui.until(lambda s: 'project API' in s.get('diagnostic', '') and not s.get('busy'))
        ui.snapshot('incompatible-project')
        ui.fill('center.open.path', str(moved / 'project.azureproject'))
        ui.click('center.open')
        ui.until(lambda s: s.get('phase') == 'Editor')
        assert ui.state()['project']['id'] == document['id']
        ui.close()
    records = json.loads(history.read_text(encoding='utf-8'))['projects']
    assert sum(r['id'] == document['id'] for r in records) == 1
    assert records[0]['path'] == str(moved / 'project.azureproject')
    with NativeUI(executable, config, output / 'restored', replay=True) as ui:
        state = ui.until(lambda s: len(s.get('recent', [])) == 3)
        assert state['recent'][0]['available']
        assert not state['recent'][1]['available'] and not state['recent'][2]['available']
        ui.snapshot('relocated-recent')
        ui.close()
    summary = dict(passed=True, missingProjectRecovery=True, incompatibleApiRecovery=True,
                   movedProjectOpened=True, relocatedHistoryDeduplicated=True, invalidRecentVisible=True,
                   chineseUserConfiguration=True, input='production Dear ImGui replay', physicalInput='not performed')
    (output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--install', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.install, args.output)
