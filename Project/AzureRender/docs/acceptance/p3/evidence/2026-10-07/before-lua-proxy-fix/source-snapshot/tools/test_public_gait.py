"""Check knee anatomy and loop continuity in shipped public locomotion."""
import base64
import json
import math
from pathlib import Path
import struct


def check(path):
    doc = json.loads(path.read_text(encoding='utf-8'))
    blob = base64.b64decode(doc['buffers'][0]['uri'].split(',', 1)[1])
    walk = next(a for a in doc['animations'] if a['name'] == 'walk')
    for channel in walk['channels']:
        name = doc['nodes'][channel['target']['node']]['name']
        if channel['target']['path'] != 'rotation':
            continue
        accessor = doc['accessors'][walk['samplers'][channel['sampler']]['output']]
        view = doc['bufferViews'][accessor['bufferView']]
        offset = view.get('byteOffset', 0) + accessor.get('byteOffset', 0)
        values = [struct.unpack_from('<4f', blob, offset+i*16) for i in range(accessor['count'])]
        assert max(abs(a-b) for a,b in zip(values[0], values[-1])) < 1e-6, (path,name,'loop')
        if name.endswith('calf'):
            angles = [2*math.atan2(v[0], v[3]) for v in values]
            assert max(angles) <= 1e-6, (path,name,'knee hyperextension',max(angles))
            assert min(angles) < -.6, (path,name,'swing knee flexion')


if __name__ == '__main__':
    root = Path(__file__).resolve().parents[1]/'assets_public'
    for name in ['third_person/explorer.gltf','third_person/guide.gltf',
                 'exploration/assets/hero.gltf','exploration/assets/guide.gltf']:
        check(root/name)
    print('Public knees and loop continuity: passed')
