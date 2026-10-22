"""Verify complete bridge pipeline pages and retain frame-scoped binding identity."""
import argparse
import json
from pathlib import Path

from .renderdoc_capture import fingerprint


def build_manifest(capture, inventory_path, pipeline_paths):
    capture = Path(capture).resolve(strict=True)
    source = fingerprint(capture)
    inputs = []

    def read(path, mode):
        path = Path(path).resolve(strict=True)
        identity = fingerprint(path)
        data = json.loads(path.read_text(encoding='utf-8-sig'))
        if fingerprint(path) != identity:
            raise ValueError('Input changed while reading')
        if Path(data.get('capture_path', '')).resolve() != capture:
            raise ValueError('Inventory belongs to another capture')
        if (data.get('schema_version') != 2 or data.get('mode') != mode
                or data.get('complete') is not True or data.get('errors')):
            raise ValueError('Unsupported or incomplete inventory')
        inputs.append(identity)
        return data

    inventory = read(inventory_path, 'resources')
    api = inventory.get('api')
    if not api or 'Unknown' in api:
        raise ValueError('Missing capture API')
    actions = {}
    for action in inventory['actions']:
        if action['eid'] in actions:
            raise ValueError('Duplicate inventory event')
        actions[action['eid']] = action
    expected = {eid for eid, action in actions.items() if action['kind'] in ('Draw', 'Dispatch')}
    resources = {item['resourceId']: item for item in inventory['resources']}
    events, seen = [], set()
    previous, more = 0, True
    for path in pipeline_paths:
        page = read(path, 'pipelines')
        if page.get('api') != api:
            raise ValueError('Inconsistent capture API')
        if not more or page.get('after_eid') != previous:
            raise ValueError('Discontinuous pipeline page')
        for event in page['pipelines']:
            eid = event['eid']
            if eid in seen or eid <= previous or eid not in expected:
                raise ValueError('Unexpected or duplicate pipeline event')
            if event.get('errors') or event['kind'] != actions[eid]['kind']:
                raise ValueError('Invalid or incomplete pipeline event')
            allowed = {'CS'} if event['kind'] == 'Dispatch' else {'VS', 'HS', 'DS', 'GS', 'PS'}
            if not event['stages'] or not set(event['stages']).issubset(allowed):
                raise ValueError('Retained or missing action stage')
            for stage in event['stages'].values():
                digest = stage.get('bytecode_sha256', '')
                if len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
                    raise ValueError('Missing shader identity')
                for category, bindings in stage['bindings'].items():
                    for binding in bindings:
                        slot = binding.get('location', {}).get('slot')
                        if type(slot) is not int or slot < 0:
                            raise ValueError('Unresolved hardware slot')
                        rid = binding.get('descriptor', {}).get('resource')
                        if category != 'sampler' and rid not in ('ResourceId::0', 'ResourceId::Null') and rid not in resources:
                            raise ValueError('Unknown bound resource: ' + str(rid))
            seen.add(eid)
            events.append(dict(event, action=actions[eid]))
            previous = eid
        if page.get('next_after_eid') != previous:
            raise ValueError('Invalid pipeline page endpoint')
        more = page.get('has_more')
        if type(more) is not bool:
            raise ValueError('Missing page continuation')
    if more or seen != expected or not expected:
        raise ValueError('Incomplete event coverage')
    if fingerprint(capture) != source:
        raise ValueError('Capture changed during indexing')
    return {'format': 'NativeCaptureBindingManifest/1', 'capture': source, 'api': api,
        'inputs': inputs, 'events': events, 'resources': inventory['resources'],
        'textures': inventory['textures'], 'buffers': inventory['buffers'],
        'coverage': {'draw_dispatch_count': len(events), 'pipeline_index_complete': True,
            'raw_payloads_exported': False,
            'scope': 'Pipeline descriptors and shader digests; raw payload exports have separate receipts'}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', required=True, type=Path)
    parser.add_argument('--inventory', required=True, type=Path)
    parser.add_argument('--pages', required=True, nargs='+', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Output already exists')
    result = build_manifest(args.capture, args.inventory, args.pages)
    with args.output.open('x', encoding='utf8') as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2, allow_nan=False)
    print(json.dumps({'path': str(args.output), 'capture': result['capture'], 'coverage': result['coverage']}))


if __name__ == '__main__':
    main()
