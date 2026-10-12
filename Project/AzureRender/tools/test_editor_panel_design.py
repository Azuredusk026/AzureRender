"""Check content and console controls through production ImGui input."""
import argparse
import json
import shutil
import uuid
from pathlib import Path
from test_editor_workspace import launch


def run(executable, output, cases=None):
    root = output / ('run-' + uuid.uuid4().hex[:8])
    root.mkdir(parents=True)
    results = []
    sources = Path(__file__).resolve().parents[1] / 'assets_public'
    for project in ('scene_inspector', 'exploration'):
        work = root / project
        source = sources / project
        if project == 'exploration':
            from create_playable_project import create
            create(work / 'game')
        else:
            shutil.copytree(source, work / 'game', ignore=shutil.ignore_patterns('.azure'))
        baseline = launch(executable, work, 'baseline', (1920, 1080), 1, capture=True)
        assert 'consoleView' in baseline, 'Console must expose its actual visible log snapshot'
        assert 'contentView' in baseline, 'Content browser must expose its actual filter and result state'
        for label, size, dpi in [('normal', (1920, 1080), 1), ('medium', (1280, 720), 1), ('compact', (1120, 720), 1), ('dpi', (1920, 1080), 1.5)]:
            if cases and label not in cases:
                continue
            actions = [dict(frame=25, action='click', target='tab.console' if label == 'compact' else 'tool.console'),
                       dict(frame=29, action='click', target='console.search'),
                       dict(frame=33, action='text', text='Opened scene'),
                       dict(frame=38, action='click', target='console.pause')]
            if label == 'compact':
                actions.insert(0, dict(frame=23, action='mouse', target='tab.console', down=False))
            console = launch(executable, work, 'console-' + label, size, dpi, actions, capture=True)
            logs = console['consoleView']
            assert logs['paused'] and logs['visibleCount'] > 0, logs
            assert all('Opened scene' in row['text'] for row in logs['visible']), logs
            assert all(row['level'] == 'info' for row in logs['visible']), logs
            assert 'console.copy' in console['widgets']
            missing = [dict(frame=25, action='click', target='tab.assets' if label == 'compact' else 'tool.assets'),
                       dict(frame=29, action='click', target='assets.search'),
                       dict(frame=33, action='text', text='无匹配_UI5')]
            if label == 'compact':
                missing.insert(0, dict(frame=23, action='mouse', target='tab.assets', down=False))
            empty = launch(executable, work, 'empty-' + label, size, dpi, missing, capture=True)
            assert empty['contentView']['matchingAssets'] == 0
            if label == 'compact':
                assert empty['contentView']['compact'], 'Compact case must exercise the reduced dock height'
            assert 'assets.clear-filters' in empty['widgets'] and not empty['dirty']
            restored = launch(executable, work, 'clear-' + label, size, dpi, missing + [
                dict(frame=40, action='click', target='assets.clear-filters')], capture=False)
            assert restored['contentView']['matchingAssets'] > 0 and not restored['contentView']['search']
            assert not restored['dirty']
            results.append(dict(project=project, size=size, dpi=dpi, consoleSearch=True,
                                pause=True, emptyFilter=True, clearFilter=True))
        if cases and 'diagnostics' in cases:
            missing_path = work / '缺失的资源.gltf'
            failure = [dict(frame=25, action='click', target='assets.import-create'),
                       dict(frame=29, action='click', target='import.path'),
                       dict(frame=33, action='text', text=str(missing_path)),
                       dict(frame=39, action='click', target='asset.import'),
                       dict(frame=45, action='mouse', target='tab.console', down=False),
                       dict(frame=47, action='click', target='tab.console'),
                       dict(frame=53, action='click', target='console.search'),
                       dict(frame=57, action='text', text='导入错误')]
            compact_error = launch(executable, work, 'compact-error', (1120,720), 1, failure, capture=True)
            assert compact_error['feedback'], 'Failed import must publish an actionable error'
            assert compact_error['consoleView']['search'] == '导入错误', 'Active diagnostics must keep the compact log toolbar accessible'
            identity = compact_error['feedback'][-1]['id']
            dismissed = launch(executable, work, 'compact-dismiss', (1120,720), 1, failure + [
                dict(frame=63, action='click', target='console.diagnostics'),
                dict(frame=69, action='click', target='feedback.dismiss.' + identity)], capture=True)
            assert not dismissed['feedback'], 'Compact diagnostic popup must dismiss the selected error'
            results.append(dict(project=project, size=[1120,720], activeError=True, toolbarAccessible=True, errorDismiss=True))
        if cases == ['diagnostics']:
            continue
        level = launch(executable, work, 'console-level', (1920,1080), 1, [
            dict(frame=25, action='click', target='tool.console'),
            dict(frame=29, action='click', target='console.level'),
            dict(frame=34, action='click', target='console.level.error')], capture=False)
        assert level['consoleView']['minimum'] == 'error'
        assert all(row['level'] == 'error' for row in level['consoleView']['visible'])
    summary = dict(passed=True, workflows=results, levelFilter=cases != ['diagnostics'],
                   input='production ImGui replay', physicalInput='not performed')
    (root / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
    (output / 'latest.json').write_text(json.dumps(dict(root=str(root), **summary), ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cases', nargs='+', choices=['normal', 'medium', 'compact', 'dpi', 'diagnostics'])
    args = parser.parse_args()
    run(args.executable.resolve(), args.output.resolve(), args.cases)
