"""Write an imported scene snapshot using explicit native source chunks."""
import argparse
import hashlib
import json
from pathlib import Path

from endfield_native.scene_sources import apply_level_source_selection


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('inventory', 'level', 'output', 'receipt'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--source-chunk', action='append', required=True)
    args = parser.parse_args()
    if args.output.resolve() == args.receipt.resolve() or any(p.exists() for p in (args.output, args.receipt)):
        parser.error('output and receipt must be distinct new files')
    inputs = [dict(path=str(p.resolve()), sha256=hashlib.sha256(p.read_bytes()).hexdigest())
              for p in (args.inventory, args.level)]
    inventory = json.loads(args.inventory.read_text(encoding='utf-8'))
    if inventory.get('format') != 'AzureEndfieldRawSceneInventory/1':
        parser.error('unsupported source inventory')
    level = json.loads(args.level.read_text(encoding='utf-8'))
    if level.get('schemaVersion') != 1:
        parser.error('unsupported scene document')
    result, receipt = apply_level_source_selection(level, inventory, args.source_chunk)
    encoded = (json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False)+'\n').encode('utf-8')
    receipt.update(generator='azure.native-scene-source-selection', generatorVersion=1,
                   inputs=inputs, outputSha256=hashlib.sha256(encoded).hexdigest())
    receipt_encoded = json.dumps(receipt, ensure_ascii=False, indent=2, allow_nan=False)+'\n'
    for p in (args.output, args.receipt):
        p.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('xb') as stream:
        stream.write(encoded)
    with args.receipt.open('x', encoding='utf-8') as stream:
        stream.write(receipt_encoded)
    print(json.dumps(dict(changedNodes=len(receipt['changedNodes']),
                         unmappedEntities=len(receipt['unmappedEntities']))))


if __name__ == '__main__':
    main()
