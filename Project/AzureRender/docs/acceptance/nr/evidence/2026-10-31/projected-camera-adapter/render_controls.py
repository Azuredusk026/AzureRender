"""Render a labeled control figure from complete GPU readback buffers."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import struct
from PIL import Image,ImageDraw,ImageFont


def channel(code,mantissa):
    exponent=code>>mantissa;fraction=code&((1<<mantissa)-1)
    if exponent==31:raise ValueError('Nonfinite packed color in mirror output')
    return math.ldexp(fraction/(1<<mantissa) if exponent==0 else 1+fraction/(1<<mantissa),(1 if exponent==0 else exponent)-15)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--font',type=Path,required=True)
    args=parser.parse_args();receipt=args.output.with_suffix('.json')
    if args.output.exists() or receipt.exists():raise FileExistsError('Figure output must be new')
    source=json.loads((args.run/'summary.json').read_text(encoding='utf-8'));assert source['status']=='passed'
    figure=Image.new('RGB',(1024,582),'#f4f5f7');draw=ImageDraw.Draw(figure);font=ImageFont.truetype(str(args.font),18)
    report=dict(shaderSha256=source['shaderSha256'],executableSha256=source['executableSha256'],sourceRun=str(args.run),panels=[])
    for column,(mode,label) in enumerate((('positive','正确参数'),('wrong-convention','方向不匹配'),('wrong-motion','运动编码不匹配'))):
        color=(args.run/mode/'0.bin').read_bytes();footprint=(args.run/mode/'1.bin').read_bytes();assert len(color)==len(footprint)==320*240*4
        pixels=[]
        for (word,) in struct.iter_unpack('<I',color):
            values=[channel(word&2047,6),channel((word>>11)&2047,6),channel(word>>22,5)];denominator=1-max(values)
            assert denominator>0
            pixels.append(tuple(round(min(1,max(0,value/denominator))**(1/2.2)*255) for value in values))
        image=Image.new('RGB',(320,240));image.putdata(pixels)
        attenuation=Image.new('L',(320,240));attenuation.putdata([round(r*255/65535) for r,g in struct.iter_unpack('<HH',footprint)])
        x=16+column*336;draw.text((x,5),label,font=font,fill='#17212b');draw.text((x,29),'颜色信号',font=font,fill='#35485a');figure.paste(image,(x,54))
        draw.text((x,305),'运动衰减',font=font,fill='#35485a');figure.paste(attenuation.convert('RGB'),(x,332))
        for y in (54,332):draw.rectangle((x+96,y+192,x+223,y+210),outline='#ffbf00',width=1)
        report['panels'].append(dict(mode=mode,colorSha256=hashlib.sha256(color).hexdigest(),footprintSha256=hashlib.sha256(footprint).hexdigest()))
    args.output.parent.mkdir(parents=True,exist_ok=True);figure.save(args.output)
    report['figureSha256']=hashlib.sha256(args.output.read_bytes()).hexdigest();receipt.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('GPU control figure saved:',args.output)


if __name__=='__main__':main()
