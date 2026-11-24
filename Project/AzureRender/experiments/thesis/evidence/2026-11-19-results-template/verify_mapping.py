"""Bounded verification of the thesis result-table mapping from audited exports."""
import argparse
from collections import defaultdict
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics


def read(path):
    return json.loads(path.read_bytes())


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def condition(row):
    return tuple(str(row[field]) for field in ('device', 'width', 'height', 'complexity', 'path'))


def close(actual, expected):
    assert math.isclose(float(actual), expected, rel_tol=1e-12, abs_tol=1e-12), (actual, expected)


def verify(run_dir, export_dir, output_dir):
    checkpoint = (export_dir/'checkpoint-run-summary.json').exists()
    prefix = 'checkpoint-' if checkpoint else ''
    summary_path = export_dir/f'{prefix}run-summary.json'
    condition_path = export_dir/f'{prefix}condition-summary.csv'
    summary = read(summary_path)
    scope = 'paused-checkpoint' if checkpoint else 'formal-complete'
    assert summary['dataScope'] == scope
    index_path = run_dir/'formal-index.json'
    plan_path = run_dir/'formal-plan.json'
    index = read(index_path)
    plan = read(plan_path)
    assert summary['planSha256'] == digest(plan_path) == index['planSha256']
    assert summary['indexSha256'] == digest(index_path)
    runs = summary['runs']
    assert len(runs) == len(index['accepted']) == summary['independentRuns']
    assert summary['measuredFrames'] == len(runs)*plan['frames']
    assert summary['completedBlocks']*3 == len(runs)
    assert len(runs) == (90 if checkpoint else 120)
    grouped = defaultdict(list)
    by_block = defaultdict(dict)
    accepted = {(row['block'], row['path']):row for row in index['accepted']}
    assert len(accepted) == len(runs)
    environment_fields = set()
    for row in runs:
        grouped[condition(row)].append(row)
        assert row['path'] not in by_block[row['block']]
        by_block[row['block']][row['path']] = row
        original = accepted[(row['block'],row['path'])]
        assert all(row[field] == original[field] for field in (
            'device', 'width', 'height', 'complexity', 'repeat', 'raw', 'rawSha256'))
        block = plan['blocks'][row['block']-1]
        assert all(row[field] == block[field] for field in ('device','width','height','complexity','repeat'))
        assert block['paths'][original['position']-1] == row['path']
        assert original['startedUtc']
        assert all(field in plan['devices'][row['device']] for field in ('deviceName','apiVersion','driverVersion'))
        for boundary in ('before', 'after'):
            env = original[boundary]
            assert env['supply'] == plan['devices'][row['device']]['supply']
            if row['device'] == 'pixel6':
                assert env['thermalStatus'] == 0 and env['sensors']
                assert all('temperatureC' in sensor for sensor in env['sensors'].values())
                environment_fields.update(('supply', 'thermalStatus', 'sensors.*.temperatureC'))
            else:
                fields = ('temperature.gpu','power.draw','clocks.current.graphics','clocks.current.memory',
                          'clocks_event_reasons.sw_thermal_slowdown','clocks_event_reasons.hw_thermal_slowdown')
                assert all(field in env['gpu'] for field in fields)
                environment_fields.update('gpu.'+field for field in fields)
        model = row['attachmentModel']
        pixels = row['width']*row['height']
        assert model['logicalBytes'] == 80*pixels
        assert model['localReuseScenarioBytes'] == (80 if row['path']=='traditional' else 64)*pixels
        assert model['hardwareMeasured'] is False and model['unit'] and model['assumptions']
        assert row['gpu']['n'] == row['cpuRecording']['n'] == plan['frames'] == 1000
    assert all(set(block)=={'traditional','subpasses','drlr'} for block in by_block.values())
    with condition_path.open(newline='', encoding='utf-8') as file:
        table = list(csv.DictReader(file))
    assert len(table) == len(grouped) == 24
    assert len({condition(row) for row in table}) == len(table)
    metric_fields = []
    paired_fields = []
    for entry in table:
        items = grouped[condition(entry)]
        assert entry['dataScope'] == scope
        assert int(entry['independentRuns']) == len(items)
        assert int(entry['plannedIndependentRuns']) == plan['repeatCount'] == 5
        assert int(entry['measuredFramesPerRun']) == plan['frames']
        for source, label in (('gpu','gpu'), ('cpuRecording','cpu')):
            for metric in ('meanMs','medianMs','p95Ms','p99Ms','varianceMs2'):
                field = f'{label}MedianRun{metric[0].upper()}{metric[1:]}'
                close(entry[field],statistics.median(row[source][metric] for row in items))
                metric_fields.append(field)
        models = {row['attachmentModel']['localReuseScenarioBytes'] for row in items}
        assert models == {int(entry['modelBytesPerFrame'])}
        baselines = [by_block[row['block']]['traditional'] for row in items]
        pairs = [100*(row['gpu']['meanMs']/base['gpu']['meanMs']-1)
                 for row,base in zip(items,baselines,strict=True)]
        for suffix, value in (('Median',statistics.median(pairs)),('Min',min(pairs)),('Max',max(pairs))):
            field = f'pairedGpuMeanChange{suffix}Pct'
            close(entry[field],value)
            paired_fields.append(field)
        lower = sum(row['gpu']['varianceMs2'] < base['gpu']['varianceMs2']
                    for row,base in zip(items,baselines,strict=True))
        assert int(entry['gpuVarianceLowerThanTraditionalRuns']) == lower
        if entry['path'] == 'drlr':
            pairs = [100*(row['gpu']['meanMs']/by_block[row['block']]['subpasses']['gpu']['meanMs']-1)
                     for row in items]
            for suffix, value in (('Median',statistics.median(pairs)),('Min',min(pairs)),('Max',max(pairs))):
                field = f'pairedGpuMeanChangeVsSubpasses{suffix}Pct'
                close(entry[field],value)
                paired_fields.append(field)
        else:
            assert all(entry[f'pairedGpuMeanChangeVsSubpasses{suffix}Pct']=='' for suffix in ('Median','Min','Max'))

    example_run = next(row for row in runs if row['path']=='drlr')
    subpasses = by_block[example_run['block']]['subpasses']
    model = example_run['attachmentModel']
    scope_label = '流程核对示例，已接受 90/120 次运行' if checkpoint else '完整导出的字段映射核对示例，120/120 次运行'
    example = dict(label=scope_label, dataScope=scope, figureReady=False,
                   condition=dict(zip(('device','width','height','complexity','path'),condition(example_run),strict=True)),
                   block=example_run['block'], repeat=example_run['repeat'],
                   H1=dict(logicalMiBPerFrame=model['logicalBytes']/1048576,
                           localScenarioMiBPerFrame=model['localReuseScenarioBytes']/1048576,
                           scenarioChangePct=100*(model['localReuseScenarioBytes']/model['logicalBytes']-1),
                           hardwareMeasured=model['hardwareMeasured']),
                   H2=dict(pairedGpuMeanVsSubpassesPct=100*(example_run['gpu']['meanMs']/subpasses['gpu']['meanMs']-1)),
                   H3=dict(gpuVarianceMs2=example_run['gpu']['varianceMs2'],
                           modelMiBPerFrame=model['localReuseScenarioBytes']/1048576,
                           measuredFramesInRun=example_run['gpu']['n']))
    sources = [summary_path,condition_path,index_path,plan_path,Path(__file__).resolve()]
    receipt = dict(label=scope_label, dataScope=scope, completedBlocks=summary['completedBlocks'],
                   acceptedRuns=len(runs), measuredFrames=summary['measuredFrames'],
                   conditionRows=len(table),metricFields=sorted(set(metric_fields)),
                   pairedFields=sorted(set(paired_fields)),environmentFields=sorted(environment_fields),
                   checks=['all condition medians','all block-paired percentage ranges',
                           'all paired variance counts','all model units and values','all environment joins'],
                   sources=[dict(path=path.as_posix(),sha256=digest(path)) for path in sources])
    output_dir.mkdir(parents=True,exist_ok=True)
    for name, data in (('mapping-verification.json',receipt),('mapping-example.json',example)):
        (output_dir/name).write_text(json.dumps(data,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    print(f'{scope}: verified {len(table)} condition rows and {len(runs)} run mappings')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--run-dir',type=Path,required=True)
    parser.add_argument('--export-dir',type=Path,required=True)
    parser.add_argument('--output-dir',type=Path,required=True)
    args = parser.parse_args()
    verify(args.run_dir,args.export_dir,args.output_dir)
