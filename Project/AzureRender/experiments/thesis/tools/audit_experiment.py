"""Verify pilot/formal evidence and export independent-run statistics."""
import argparse
from collections import Counter, defaultdict
import csv
from datetime import datetime
import gzip
import hashlib
import json
from pathlib import Path
import statistics

from analysis import PATHS, summarize


def sha(data):
    return hashlib.sha256(data).hexdigest()


def key(row):
    return tuple(row[name] for name in ('device','width','height','complexity','path'))


def qualified(device, env):
    if device == 'pixel6':
        return env['thermalStatus'] == 0
    return all(env['gpu'][field] == 'Not Active' for field in (
        'clocks_event_reasons.sw_thermal_slowdown','clocks_event_reasons.hw_thermal_slowdown'))


def verify_schedule(plan, index, pause):
    blocks = plan['blocks']
    if [block['block'] for block in blocks] != list(range(1, 41)) or any(
        sorted(block['paths']) != sorted(PATHS) for block in blocks
    ):
        raise ValueError('expected 40 consecutive blocks of three distinct paths')
    expected = [(block['block'], position, path) for block in blocks
                for position, path in enumerate(block['paths'], 1)]
    actual = [(row['block'], row['position'], row['path']) for row in index['accepted']]
    if pause is None:
        if index['status'] != 'passed' or actual != expected:
            raise ValueError('formal output requires a passed index with all 120 scheduled runs')
        return len(blocks)

    if index['status'] != 'running' or not 0 < len(actual) < len(expected) or len(actual) % 3:
        raise ValueError('checkpoint requires an in-progress index with whole accepted blocks')
    completed = len(actual) // 3
    receipt = dict(status='paused', planSha256=index['planSha256'], acceptedRuns=len(actual),
                   completedBlocks=completed, measuredFrames=plan['frames']*len(actual),
                   rejectedBlocks=len(index['rejected']), nextBlock=completed+1,
                   remainingRuns=len(expected)-len(actual))
    if actual != expected[:len(actual)] or any(pause.get(name) != value for name, value in receipt.items()):
        raise ValueError('checkpoint differs from the frozen schedule or the accepted index')
    return completed


def audit(directory, pilot_only, output, checkpoint=False):
    def read(name):
        return json.loads((directory/name).read_bytes())

    pilot = read('pilot-index.json')
    assert pilot['status'] == 'passed' and len(pilot['cases']) == 24
    pilot_rows = {key(row): row for row in pilot['cases']}
    assert len(pilot_rows) == 24
    images = defaultdict(set)
    for condition, row in pilot_rows.items():
        data = (directory/row['raw']).read_bytes()
        assert sha(data) == row['rawSha256']
        run = json.loads(data)
        assert run['warmup'] == 30 and summarize(run)['gpu']['n'] == 60
        assert row['environmentPass'] and row['before']['supply'] == row['after']['supply']
        assert all(qualified(row['device'], row[boundary]) for boundary in ('before', 'after'))
        assert condition[1:] == tuple(run[field] for field in ('width','height','complexity','path'))
        assert not (run['validationEnabled'] or run['captureEnabled'] or run['presentationWorkMs'])
        assert run['dynamicRenderingLocalReadEnabled'] == (row['path'] == 'drlr')
        images[condition[:-1]].add(row['imageSha256'])
    assert len(images) == 8 and all(len(group) == 1 for group in images.values())
    print('Verified 24 pilot records and eight within-device image groups')
    if pilot_only:
        return

    plan = read('formal-plan.json')
    result = read('formal-index.json')
    assert result['planSha256'] == sha((directory/'formal-plan.json').read_bytes())
    assert plan['artifacts']['pilotIndexSha256'] == sha((directory/'pilot-index.json').read_bytes())
    capability = directory/'pixel6-capabilities.json'
    if not capability.exists():
        capability = directory.parent/'pixel6-qualification/pixel6-capabilities.json'
    assert plan['artifacts']['pixel6CapabilitySha256'] == sha(capability.read_bytes())
    assert plan['artifacts']['runnerSha256'] == sha((directory/'experiment.py').read_bytes())
    evidence = Path(__file__).resolve().parents[1]/'evidence'
    for source, field in (('android-build','androidBuildManifestSha256'), ('swapchain','windowsBuildManifestSha256')):
        assert plan['artifacts'][field] == sha((evidence/source/'manifest.json').read_bytes())
    payload = read('payload.json')
    assert len(payload['shaders']) == 7 and payload['shaders'] == plan['artifacts']['shaders']
    assert payload['benchmarkSha256'] == plan['artifacts']['binaries']['pixel6']
    assert plan['settings'] == dict(capture=False, validation=False, present=False, processesPerBlock=3)
    blocks = {block['block']: block for block in plan['blocks']}
    assert len(blocks) == 40 and plan['warmup'] == 300 and plan['frames'] == 1000 and plan['repeatCount'] == 5
    pause = read('pause-checkpoint.json') if checkpoint else None
    completed = verify_schedule(plan, result, pause)
    planned = Counter(key(dict(block, path=path)) for block in blocks.values() for path in block['paths'])
    assert planned == {condition:5 for condition in pilot_rows}
    scope = 'paused-checkpoint' if checkpoint else 'formal-complete'
    seen = set()
    runs = []
    groups = defaultdict(list)
    prior_start = datetime.fromisoformat(plan['frozenUtc'])
    raw_files = set()
    for row in result['accepted']:
        block = blocks[row['block']]
        assert all(row[name] == block[name] for name in ('repeat','device','width','height','complexity'))
        reference = plan['devices'][row['device']]
        assert all(row[name] == reference[name] for name in ('deviceName','apiVersion','driverVersion'))
        for boundary in ('before','after'):
            assert row[boundary]['supply'] == reference['supply'] and qualified(row['device'],row[boundary])
        start = datetime.fromisoformat(row['startedUtc'])
        assert start >= prior_start and row['wallSeconds'] > 0
        prior_start = start
        assert row['imageSha256'] == pilot_rows[key(row)]['imageSha256']
        raw_files.add(row['raw'])
        data = gzip.decompress((directory/row['raw']).read_bytes())
        digest = sha(data)
        assert digest == row['rawSha256'] and digest not in seen
        seen.add(digest)
        run = json.loads(data)
        stats = summarize(run)
        assert (run['device'],run['apiVersion'],run['driverVersion']) == tuple(reference[name] for name in ('deviceName','apiVersion','driverVersion'))
        assert key(row)[1:] == tuple(run[field] for field in ('width','height','complexity','path'))
        assert stats['gpu']['n'] == plan['frames'] and run['warmup'] == plan['warmup']
        assert min(run['gpuMs']) > 0 and run['timestampValidBits'] > 0 and run['timestampPeriodNs'] > 0
        assert not (run['validationEnabled'] or run['captureEnabled'] or run['presentationWorkMs'])
        assert run['dynamicRenderingLocalReadEnabled'] == (row['path'] == 'drlr')
        summary = {name:row[name] for name in ('device','width','height','complexity','path','repeat','block','raw','rawSha256')}
        summary.update(gpu=stats['gpu'], cpuRecording=stats['cpuRecording'], attachmentModel=stats['traffic'])
        runs.append(summary)
        groups[key(row)].append(summary)
    for reject in result['rejected']:
        for row in reject['observations']:
            data = gzip.decompress((directory/row['raw']).read_bytes())
            assert sha(data) == row['rawSha256'] and row['raw'] not in raw_files
            raw_files.add(row['raw'])
    assert raw_files == {str(path.relative_to(directory).as_posix()) for path in (directory/'formal').glob('*.json.gz')}

    columns = ('device','width','height','complexity','path')
    conditions = []
    for group, expected in sorted(planned.items()):
        items = groups[group]
        entry = dict(zip(columns,group,strict=True), dataScope=scope, independentRuns=len(items),
                     plannedIndependentRuns=expected, measuredFramesPerRun=plan['frames'])
        if not items:
            conditions.append(entry)
            continue
        for source, label in (('gpu','gpu'),('cpuRecording','cpu')):
            for metric in ('meanMs','medianMs','p95Ms','p99Ms','varianceMs2'):
                entry[f'{label}MedianRun{metric[0].upper()}{metric[1:]}'] = statistics.median(item[source][metric] for item in items)
        entry['modelBytesPerFrame'] = items[0]['attachmentModel']['localReuseScenarioBytes']
        baseline = {item['repeat']:item for item in groups[(*group[:-1],'traditional')]}
        paired = [100*(item['gpu']['meanMs']/baseline[item['repeat']]['gpu']['meanMs']-1) for item in items]
        entry.update(pairedGpuMeanChangeMedianPct=statistics.median(paired), pairedGpuMeanChangeMinPct=min(paired),
                     pairedGpuMeanChangeMaxPct=max(paired))
        entry['gpuVarianceLowerThanTraditionalRuns'] = sum(
            item['gpu']['varianceMs2'] < baseline[item['repeat']]['gpu']['varianceMs2'] for item in items)
        if group[-1] == 'drlr':
            subpasses = {item['repeat']:item for item in groups[(*group[:-1],'subpasses')]}
            relative = [100*(item['gpu']['meanMs']/subpasses[item['repeat']]['gpu']['meanMs']-1) for item in items]
            entry['pairedGpuMeanChangeVsSubpassesMedianPct'] = statistics.median(relative)
            entry['pairedGpuMeanChangeVsSubpassesMinPct'] = min(relative)
            entry['pairedGpuMeanChangeVsSubpassesMaxPct'] = max(relative)
        conditions.append(entry)
    prefix = 'checkpoint-' if checkpoint else ''
    output.mkdir(parents=True, exist_ok=True)
    (output/f'{prefix}run-summary.json').write_text(json.dumps(dict(dataScope=scope,
        planSha256=result['planSha256'], indexSha256=sha((directory/'formal-index.json').read_bytes()),
        completedBlocks=completed, plannedBlocks=len(blocks), independentRuns=len(runs),
        plannedIndependentRuns=sum(planned.values()), measuredFrames=len(runs)*plan['frames'],
        rejectedBlocks=len(result['rejected']), runs=runs),indent=2)+'\n',encoding='utf-8')
    with (output/f'{prefix}condition-summary.csv').open('w',newline='',encoding='utf-8') as file:
        writer=csv.DictWriter(file,fieldnames=list(dict.fromkeys(field for row in conditions for field in row)))
        writer.writeheader()
        writer.writerows(conditions)
    print(f'{scope}: verified {len(runs)} runs, {completed}/{len(blocks)} blocks, '
          f'{len(runs)*plan["frames"]} measured frames, {len(result["rejected"])} rejected blocks')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--output',type=Path)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--pilot-only',action='store_true')
    modes.add_argument('--checkpoint',action='store_true')
    args=parser.parse_args()
    if not args.pilot_only and args.output is None:
        parser.error('--output is required when exporting run statistics')
    try:
        audit(args.directory,args.pilot_only,args.output,args.checkpoint)
    except ValueError as error:
        parser.exit(1,f'{error}\n')
