"""Restore only explicitly authored opaque surfaces from the native source audit."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys


def restore_opaque_surfaces(document, audit):
    by_name = {}
    for row in audit['materials']:
        name = row['object']['name']
        if name in by_name:
            raise ValueError('Ambiguous native material name: ' + name)
        by_name[name] = row
    result = copy.deepcopy(document)
    report = dict(restored=[], unresolved=[], shader_equivalence_verified=False)
    for index, material in enumerate(result.get('materials', [])):
        name = material.get('name', '')
        row = by_name.get(name)
        values = {v['name']: v['value'] for v in row['floats']} if row else {}
        required = dict(_SurfaceType=0, _SrcBlend=1, _DstBlend=0, _AlphaClip=0, _EnableAlphaTest=0)
        if not all(values.get(k) == v for k,v in required.items()):
            report['unresolved'].append(dict(slot=index, name=name, reason='opaque_contract_not_established'))
            continue
        material['alphaMode'] = 'OPAQUE'
        material.pop('alphaCutoff', None)
        material.setdefault('extras', {})['nativeOpaqueSource'] = row['object']
        report['restored'].append(dict(slot=index, name=name, source=row['object'], parameters=required))
    return result, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--source-audit', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    outputs = [args.output.resolve(), args.report.resolve()]
    inputs = [args.input.resolve(), args.source_audit.resolve()]
    if len(set(outputs)) != 2 or any(p.exists() or p in inputs for p in outputs):
        raise ValueError('Outputs must be distinct new files')
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from convert_material_semantics import read_asset, write_asset
    document, tail = read_asset(args.input)
    result, report = restore_opaque_surfaces(document, json.loads(args.source_audit.read_text(encoding='utf8')))
    report['inputs'] = [dict(path=str(p), sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in inputs]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    write_asset(args.output, result, tail)
    report['output_sha256'] = hashlib.sha256(args.output.read_bytes()).hexdigest()
    args.report.write_text(json.dumps(report, indent=2), encoding='utf8')
    print(json.dumps(dict(restored=len(report['restored']), unresolved=len(report['unresolved']))))


if __name__ == '__main__':
    main()
