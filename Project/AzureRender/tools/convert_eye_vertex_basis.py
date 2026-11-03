"""Convert an exported glTF eye mesh to the radial material's native UV basis."""
import argparse
import copy
import os
from pathlib import Path
import struct
import tempfile

import numpy as np

from convert_material_semantics import read_asset, slots, write_asset
from endfield_native.character_tangents import accessor


def convert_eye_basis(document, tail, slot):
    identities = slots(document)
    if slot not in identities:
        raise ValueError('Unknown eye material slot')
    material = identities.index(slot)
    profile = document['materials'][material].get('extras', {}).get('azureRenderMaterial', {})
    if profile.get('class') != 'eye':
        raise ValueError('The selected slot must declare the eye material class')
    if (len(document.get('buffers', [])) != 1 or len(tail) < 8
            or struct.unpack_from('<II', tail) != (len(tail) - 8, 0x004e4942)):
        raise ValueError('Expected one embedded GLB binary chunk')

    result = copy.deepcopy(document)
    payload = bytearray(tail[8:])
    converted = {}
    for mesh in result['meshes']:
        for primitive in mesh['primitives']:
            if primitive.get('material') != material:
                continue
            extras = primitive.setdefault('extras', {})
            if 'azureEyeVertexBasis' in extras:
                raise ValueError('Eye primitive already declares a vertex basis')
            attrs = primitive['attributes']
            for name, kind in [('TEXCOORD_0', 'VEC2'), ('TANGENT', 'VEC4')]:
                source = attrs.get(name)
                if source is None:
                    raise ValueError('Eye primitive is missing '+name)
                key = (name, source)
                if key not in converted:
                    layout = document['accessors'][source]
                    if layout['componentType'] != 5126 or layout['type'] != kind:
                        raise ValueError('Expected floating-point '+name)
                    values = accessor(document, tail, source).copy()
                    if not np.isfinite(values).all():
                        raise ValueError('Non-finite eye vertex input')
                    if name == 'TEXCOORD_0':
                        values[:, 1] = 1 - values[:, 1]
                    else:
                        if not np.isin(values[:, 3], [-1, 1]).all():
                            raise ValueError('Invalid eye tangent handedness')
                        values[:, 3] *= -1
                    payload.extend(bytes(-len(payload) % 4))
                    view = len(result['bufferViews'])
                    result['bufferViews'].append(dict(buffer=0, byteOffset=len(payload),
                                                      byteLength=values.nbytes, target=34962))
                    payload.extend(values.tobytes())
                    converted[key] = len(result['accessors'])
                    result['accessors'].append(dict(bufferView=view, componentType=5126,
                                                     type=kind, count=len(values)))
                attrs[name] = converted[key]
            extras['azureEyeVertexBasis'] = 'native-radial'
    if not converted:
        raise ValueError('No primitives use the selected eye material')
    result['buffers'][0]['byteLength'] = len(payload)
    return result, struct.pack('<II', len(payload), 0x004e4942) + payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--slot', required=True)
    args = parser.parse_args()
    if args.input.suffix.lower() != '.glb' or args.output.suffix.lower() != '.glb':
        parser.error('Input and output must be GLB files')
    if args.output.exists():
        parser.error('Output already exists')
    document, tail = read_asset(args.input)
    result, data = convert_eye_basis(document, tail, args.slot)
    with tempfile.TemporaryDirectory(dir=args.output.parent) as directory:
        staged = Path(directory) / 'converted.glb'
        write_asset(staged, result, data)
        # Publish only a complete file, keeping concurrent outputs intact.
        os.link(staged, args.output)


if __name__ == '__main__':
    main()
