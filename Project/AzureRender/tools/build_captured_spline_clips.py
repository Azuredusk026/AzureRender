"""Build source spline motion with captured opaque mesh/material templates."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import traceback

from convert_material_semantics import read_asset, write_asset
from endfield_native.captured_spline import build_captured_emitter
from endfield_native.spline_bake import compile_emitter


def main():
    parser = argparse.ArgumentParser(__doc__)
    for name in ['input', 'inventory', 'world', 'bindings', 'output']:
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--sample-rate', type=float, default=30)
    parser.add_argument('--seed', type=int, required=True)
    args = parser.parse_args()
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    sources = [args.input, args.inventory, args.world, args.bindings]
    fingerprints = {str(p.resolve()): sha(p) for p in sources}
    document, tail = read_asset(args.input)
    if tail is None or len(tail) < 8 or tail[4:8] != b'BIN\0' or struct.unpack_from('<I', tail)[0] != len(tail)-8:
        raise ValueError('Expected captured GLB with one BIN chunk')
    inventory = json.loads(args.inventory.read_text(encoding='utf8'))
    emitters = {tuple(e['identity']): e for e in inventory['emitters']}
    worlds = {tuple(w['identity']): w for w in json.loads(args.world.read_text(encoding='utf8'))}
    bindings = json.loads(args.bindings.read_text(encoding='utf8'))
    if bindings.get('schemaVersion') != 1 or not bindings.get('emitters'):
        raise ValueError('Invalid captured spline binding format')
    identities = [tuple(row['identity']) for row in bindings['emitters']]
    if len(set(identities)) != len(identities):raise ValueError('Duplicate bound emitter identity')
    args.output.mkdir(parents=True, exist_ok=False)
    report = dict(format='NativeCapturedSplineBatch/1', status='running', inputs=fingerprints,
                  seed=args.seed, sampleRate=args.sample_rate, emitters=[])
    root = Path(__file__).resolve().parent
    report['tools'] = {str(p.relative_to(root)): sha(p) for p in [Path(__file__),
        root/'endfield_native/captured_spline.py', root/'endfield_native/spline_bake.py',
        root/'endfield_native/spline_motion.py', root/'convert_material_semantics.py']}
    try:
        for binding in bindings['emitters']:
            identity = tuple(binding['identity']);emitter = emitters[identity];name = emitter['name']
            if not name or any(c in name for c in '/\\\0') or name in ['.', '..']:
                raise ValueError('Invalid emitter output name')
            path = args.output/(name+'.glb')
            if path.exists():raise ValueError('Duplicate emitter output name')
            clip = compile_emitter(emitter, sample_rate=args.sample_rate, seed=args.seed)
            model, binary, receipt = build_captured_emitter(document, tail[8:], clip,
                worlds[identity]['candidate_world_matrix_row_major'], binding['templates'])
            write_asset(path, model, struct.pack('<I4s', len(binary), b'BIN\0')+binary)
            receipt.update(file=path.name, sha256=sha(path), bytes=path.stat().st_size,
                           capturedTemplates=binding['templates'], timing=clip['timing'])
            report['emitters'].append(receipt)
            print('BUILT', name, receipt['slotCount'], 'slots', flush=True)
        for path, digest in fingerprints.items():
            if sha(Path(path)) != digest:raise ValueError('Source changed during conversion: '+path)
        report['status'] = 'converted'
    except Exception:
        report.update(status='failed', error=traceback.format_exc())
        raise
    finally:
        (args.output/'receipt.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')


if __name__ == '__main__':
    main()
