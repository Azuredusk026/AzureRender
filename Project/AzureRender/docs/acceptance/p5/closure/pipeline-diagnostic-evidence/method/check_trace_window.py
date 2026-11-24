import gzip
import importlib.util
import json
from pathlib import Path
import tempfile

script = Path(__file__).with_name('analyse_trace_window.py')
spec = importlib.util.spec_from_file_location('window', script)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with tempfile.TemporaryDirectory(prefix='azure-trace-window-') as temporary:
    root = Path(temporary)
    def region(base, resident, path=''):
        return dict(allocation=base, kind=131072, path=path,
                    residentBytes=resident, queriedRegionBytes=4096)
    before = dict(counter=101, racedPages=0, regions=[region(1000, 4096)])
    after = dict(counter=102, racedPages=0, regions=[region(1000, 8192), region(3000, 4096)])
    observations = [dict(seconds=index*.2, counter=100+index*.2,
                         working=100 if index < 9 else 200) for index in range(10)]
    events = [dict(api=1, status=0, base=1000, inputBase=0, size=16384,
                   qpc=1010, stack=[505]),
              dict(api=3, status=0, base=9000, inputBase=9000, size=4096,
                   qpc=1015, stack=[506])]
    summary = dict(childExitCode=0, dropped=0, reentrant=0,
                   invalidArguments=0, unpublished=0, events=2,
                   counterFrequency=10, modules=[dict(base=500, size=10, path='owned.dll')],
                   command=['owned.exe'], hashes={})
    def write():
        (root/'summary.json').write_text(json.dumps(summary))
        (root/'residency.json').write_text(json.dumps(dict(observations=observations, snapshots=[before, after])))
        (root/'events.bin').write_bytes(b'fixture')
        (root/'events.json.gz').write_bytes(gzip.compress(json.dumps(events).encode()))
    write()
    module.analyse(root, root/'analysis.json')
    result = json.loads((root/'analysis.json').read_text())
    target = next(row for row in result['increasingRegions'] if row['allocation'] == 1000)
    assert target['residentIncreaseBytes'] == 4096
    assert len(target['successfulOverlappingCalls']) == 2
    assert target['successfulOverlappingCalls'][0]['stackModules'][0]['module'] == 'owned.dll'
    assert result['stableGrowth'] == 1
    summary['dropped'] = 1
    write()
    try:
        module.analyse(root, root/'invalid.json')
    except AssertionError:
        pass
    else:
        raise AssertionError('Dropped events must reject analysis')
print('Known commit, interior release, growth and coverage rejection verified')
