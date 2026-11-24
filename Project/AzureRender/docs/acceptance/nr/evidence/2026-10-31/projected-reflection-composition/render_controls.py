"""Render confidence-weighted HDR reflection readbacks with fixed ROI labels."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import struct
from PIL import Image, ImageDraw, ImageFont


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--font',type=Path,required=True)
    args=parser.parse_args();receipt=args.output.with_suffix('.json')
    if args.output.exists() or receipt.exists():raise FileExistsError('Figure outputs must be new')
    source=json.loads((args.run/'summary.json').read_text(encoding='utf-8'));assert source['status']=='passed'
    image=Image.new('RGB',(1024,606),'#f4f5f7');draw=ImageDraw.Draw(image);font=ImageFont.truetype(str(args.font),18)
    report=dict(sourceRun=str(args.run),shaderSha256=source['shaderSha256'],executableSha256=source['executableSha256'],display='linear HDR multiplied by confidence, clipped to one, gamma 1/2.2',panels=[])
    modes=[('positive','当前帧'),('history-tinted','前帧颜色'),('history-shifted','前帧相机平移'),('history-wrong-pose','前帧位姿不匹配'),('history-disoccluded','前帧遮挡变化'),('wide-edge','宽边缘衰减')]
    for i,(mode,label) in enumerate(modes):
        data=(args.run/mode/'3.bin').read_bytes();assert len(data)==320*240*16;pixels=[]
        for r,g,b,a in struct.iter_unpack('<4f',data):
            assert all(math.isfinite(v) and v>=0 for v in (r,g,b,a)) and a<=1
            pixels.append(tuple(round(min(1,v*a)**(1/2.2)*255) for v in (r,g,b)))
        panel=Image.new('RGB',(320,240));panel.putdata(pixels);x=16+i%3*336;y=10+i//3*298
        draw.text((x,y),label,font=font,fill='#17212b');draw.text((x,y+24),'可信度加权反射颜色',font=font,fill='#35485a');image.paste(panel,(x,y+50))
        draw.rectangle((x+96,y+242,x+223,y+260),outline='#ffbf00',width=1)
        report['panels'].append(dict(mode=mode,sourceSha256=hashlib.sha256(data).hexdigest()))
    args.output.parent.mkdir(parents=True,exist_ok=True);image.save(args.output);report['figureSha256']=hashlib.sha256(args.output.read_bytes()).hexdigest()
    receipt.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print('Composed reflection figure saved:',args.output)


if __name__=='__main__':main()
