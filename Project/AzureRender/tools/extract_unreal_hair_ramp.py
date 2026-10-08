"""Bake a verified linear Unreal RichCurve into a color-managed PNG ramp."""
import argparse,sys,json,struct,hashlib
from pathlib import Path
from PIL import Image

def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('--parser-root',type=Path,required=True)
    p.add_argument('--curve',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();sys.path.insert(0,str(args.parser_root.resolve()))
    from ue_asset_parser import UAsset
    from ue_props_57 import TagReader,_NestedReader
    source=args.curve.resolve();asset=UAsset(str(source));export=next(e for e in asset.exports if asset.export_class_name(e)=='CurveLinearColor')
    reader=TagReader(asset,export['serial_offset'],export['serial_size']);original=reader.read_struct_value;raw=[]
    def capture(name,size):
        if name=='RichCurve':raw.append(asset.data[reader.pos:reader.pos+size])
        return original(name,size)
    reader.read_struct_value=capture;reader.read_tags();curves=[]
    for channel in raw[:3]:
        nested=_NestedReader(reader,channel)
        if nested.fname()[0]!='Keys':raise ValueError('Unsupported curve member serialization')
        nested.read_type_nodes();size=struct.unpack_from('<i',nested.data,nested.pos)[0];nested.pos+=4
        flags=nested.data[nested.pos];nested.pos+=1
        if flags & ~0x08:raise ValueError('Unsupported native curve flags')
        count=struct.unpack_from('<i',nested.data,nested.pos)[0];nested.pos+=4
        keys=[struct.unpack_from('<3B6f',nested.data,nested.pos+i*27) for i in range(count)]
        if size!=4+count*27 or count!=2 or any(k[0]!=0 for k in keys):raise ValueError('Only verified two-key linear curves are supported')
        if keys[1][3]<=keys[0][3]:raise ValueError('Curve times must increase')
        curves.append([(k[3],k[4]) for k in keys])
    if len(curves)!=3:raise ValueError('Three RGB curves are required')
    image=Image.new('RGBA',(256,1))
    for x in range(256):
        rgb=[]
        for keys in curves:
            weight=max(0,min(1,(x/255-keys[0][0])/(keys[1][0]-keys[0][0])))
            value=max(0,min(1,weight*keys[1][1]+(1-weight)*keys[0][1]))
            encoded=12.92*value if value<=.0031308 else 1.055*value**(1/2.4)-.055;rgb.append(round(encoded*255))
        image.putpixel((x,0),tuple(rgb+[255]))
    args.output.parent.mkdir(parents=True,exist_ok=True);image.save(args.output)
    sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    evidence={'schemaVersion':1,'source':str(source),'sourceSha256':sha(source),'curves':curves,
              'interpolation':'linear','textureEncoding':'sRGB','outputSha256':sha(args.output),'generatorSha256':sha(Path(__file__))}
    args.output.with_suffix('.source.json').write_text(json.dumps(evidence,indent=2),encoding='utf-8');print(json.dumps(evidence))
if __name__=='__main__':main()
