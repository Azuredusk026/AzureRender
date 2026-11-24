"""Validate exported glTF PNG bytes using the engine's sRGB contract."""
import argparse
from io import BytesIO
import json
from pathlib import Path
import struct
from PIL import Image


def verify(root):
    raw=(root/'surface.glb').read_bytes()
    size,kind=struct.unpack_from('<II',raw,12)
    assert kind==0x4e4f534a
    gltf=json.loads(raw[20:20+size]);binary=raw[28+size:]
    material=gltf['materials'][0]
    expected=json.loads((root/'expected.json').read_text())
    def pixel(texture):
        index=gltf['textures'][texture['index']]['source']
        view=gltf['bufferViews'][gltf['images'][index]['bufferView']]
        data=binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
        image=Image.open(BytesIO(data)).convert('RGBA')
        return image.getpixel((image.width//2,image.height//2))
    def linear(v):
        v/=255
        return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4
    pbr=material['pbrMetallicRoughness']
    base=pixel(pbr['baseColorTexture'])
    emission=pixel(material['emissiveTexture'])
    strength=material.get('extensions',{}).get('KHR_materials_emissive_strength',{}).get('emissiveStrength',1)
    e_factor=material.get('emissiveFactor',[0,0,0]);b_factor=pbr.get('baseColorFactor',[1,1,1,1])
    base_linear=[linear(v)*f for v,f in zip(base[:3],b_factor)]
    emissive_linear=[linear(v)*f*strength for v,f in zip(emission[:3],e_factor)]
    orm=pixel(pbr['metallicRoughnessTexture'])
    measurements={'base_png':base,'base_linear':base_linear,'emission_png':emission,
        'emission_linear':emissive_linear,'alpha':base[3]/255*b_factor[3],
        'roughness':orm[1]/255*pbr.get('roughnessFactor',1),
        'metallic':orm[2]/255*pbr.get('metallicFactor',1)}
    errors={'base':max(abs(a-b) for a,b in zip(base_linear,expected['base'])),
        'emission':max(abs(a-b) for a,b in zip(emissive_linear,expected['emission']))}
    for key in ('alpha','roughness','metallic'):
        errors[key]=abs(measurements[key]-expected[key])
    result={'measurements':measurements,'expected':expected,'errors':errors}
    (root/'exchange-metrics.json').write_text(json.dumps(result,indent=2))
    assert max(errors.values())<.015,result
    print(json.dumps(result,indent=2))
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path)
    verify(parser.parse_args().root)
