from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
root=Path.cwd();run=Path(__file__).resolve().parent;prior=root/'build/runs/20261101-043757-material-display-transform-93d6e0'
paths=[prior/'native-final-captures/near-world-linear/frames/frame_000000.png',run/'object-shadow-captures/world/frames/frame_000000.png',run/'standalone-captures/source-standalone/frames/frame_000000.png',run/'object-shadow-captures/source/frames/frame_000000.png']
labels=['联合对象阴影启用','联合对象阴影禁用','独立对象阴影启用','独立对象阴影禁用'];canvas=Image.new('RGB',(1200,720),(20,24,28));draw=ImageDraw.Draw(canvas);font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',24)
for i,(path,label) in enumerate(zip(paths,labels)):
 x=i%2*600;y=i//2*360;draw.text((x+12,y+4),label,font=font,fill='white');canvas.paste(Image.open(path).convert('RGB').crop((350,980,1250,1460)).resize((600,320)),(x,y+40))
canvas.save(run/'object-shadow-comparison.png')
