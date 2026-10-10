"""Correlate returned NT calls with resident-region changes in one owned run."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path


def analyse(trace, output):
    summary = json.loads((trace/'summary.json').read_text())
    memory = json.loads((trace/'residency.json').read_text())
    events = json.loads(gzip.decompress((trace/'events.json.gz').read_bytes()))
    assert summary['childExitCode'] == 0
    assert all(summary[k] == 0 for k in ('dropped', 'reentrant', 'invalidArguments', 'unpublished'))
    assert len(events) == summary['events']
    observations = memory['observations']
    assert len(observations) >= 10 and all(row['working'] > 0 for row in observations)
    stable = observations[len(observations)//2:]
    peak = max(stable, key=lambda row: row['working'])
    snapshots = memory['snapshots']
    assert all('counter' in row for row in observations), 'Exact observation counters are required'
    before = max((row for row in snapshots if row['counter'] <= stable[0]['counter']),
                 key=lambda row: row['counter'], default=snapshots[0])
    after = min((row for row in snapshots if row['counter'] >= peak['counter']),
                key=lambda row: row['counter'], default=snapshots[-1])
    if after['counter'] < before['counter']:
        before, after = after, before
    modules = summary['modules']

    def resolve(event):
        return [dict(address=address, module=module['path'], offset=address-module['base'])
                for address in event['stack'] for module in modules
                if module['base'] <= address < module['base']+module['size']]

    def identity(row):
        return (row['allocation'], row['kind'], row['path'])

    old = {identity(row): row for row in before['regions']}
    changed = []
    for region in after['regions']:
        previous = old.get(identity(region), {})
        delta = region['residentBytes']-previous.get('residentBytes', 0)
        if delta <= 0:
            continue
        base = region['allocation']
        # queriedRegionBytes sums committed regions that contain resident
        # pages; it is not a contiguous allocation address range.
        candidates = [event['size'] for event in events
                      if event['api'] in (1, 2) and not event['status'] & 0x80000000
                      and event['base'] == base
                      and event['qpc']/summary['counterFrequency'] <= after['counter']]
        size = max(candidates, default=1)
        matched = [dict(index=index, **event, stackModules=resolve(event))
                   for index, event in enumerate(events)
                   if not event['status'] & 0x80000000
                   and event['qpc']/summary['counterFrequency'] <= after['counter']
                   and (base <= event['base'] < base+size
                        or base <= event['inputBase'] < base+size)]
        changed.append(dict(**region, candidateAllocationSpanBytes=size,
                            previousResidentBytes=previous.get('residentBytes', 0),
                            residentIncreaseBytes=delta, successfulOverlappingCalls=matched))
    changed.sort(key=lambda row: row['residentIncreaseBytes'], reverse=True)
    command = summary['command']
    arguments = {}
    for option in ('--project', '--game-actions', '--gpu-timing-output'):
        if option in command:
            path = Path(command[command.index(option)+1])
            arguments[option] = dict(path=str(path), exists=path.is_file(),
                                     sha256=hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None)
    result = dict(diagnosticOnly=True, rootCauseConfirmed=False, completeAllocationCoverage=False,
                  observations=len(observations), stableFirst=stable[0], stablePeak=peak,
                  stableGrowth=peak['working']/stable[0]['working']-1,
                  beforeCounter=before['counter'], afterCounter=after['counter'],
                  beforeCounterDistance=before['counter']-stable[0]['counter'],
                  afterCounterDistance=after['counter']-peak['counter'],
                  beforeRacedPages=before['racedPages'], afterRacedPages=after['racedPages'],
                  increasingRegions=changed, command=command, argumentFiles=arguments,
                  sourceHashes=summary['hashes'],
                  evidenceHashes={name: hashlib.sha256((trace/name).read_bytes()).hexdigest()
                                  for name in ('summary.json', 'events.bin', 'events.json.gz', 'residency.json')},
                  limits=[
                      'Snapshots bracket the counter observation window; transient peak residency may escape both snapshots.',
                      'Allocation address reuse is possible; overlapping calls are candidates, not a complete live ledger.',
                      'Module snapshots may miss short-lived modules, and stack capture has a fixed depth.',
                      'Heap suballocations, mapped allocations and kernel ownership are outside coverage.',
                      'Frame-level QPC markers are unavailable in this source; exact frame association is unverified.',
                      'Diagnostic instrumentation changes timing and working set; formal runs remain separate.'])
    output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(dict(stableGrowth=result['stableGrowth'], growingRegions=len(changed),
                          top=[dict(base=hex(row['allocation']), kind=row['kind'], path=row['path'],
                                    delta=row['residentIncreaseBytes'], calls=len(row['successfulOverlappingCalls']))
                               for row in changed[:8]])))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--trace', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    analyse(args.trace, args.output)
