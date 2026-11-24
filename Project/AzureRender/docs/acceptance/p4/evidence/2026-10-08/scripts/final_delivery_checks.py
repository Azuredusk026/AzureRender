from pathlib import Path
import subprocess,sys,time
root=Path.cwd();work=root/'build/p4';exe=root/'build/ninja-msvc-release/AzureRender.exe'
commands=[
 ('workspace-input',[sys.executable,'tools/test_editor_workspace.py','--executable',str(exe),'--output',str(work/('workspace-input/run-'+str(time.time_ns())))]),
 ('workspace',[sys.executable,'build/p4/workspace_matrix.py','--executable',str(exe),'--output','build/p4/workspace']),
 ('workflow-supplement',[sys.executable,'build/p4/workflow_supplement.py']),
 ('delivery',[sys.executable,'build/p4/install_delivery.py']),
 ('intel',[sys.executable,'build/p4/intel.py'])]
for name,command in commands:
 print('START',name,flush=True)
 with (work/(name+'.log')).open('w',encoding='utf-8') as output:
  result=subprocess.run(command,stdout=output,stderr=subprocess.STDOUT)
 print('END',name,result.returncode,flush=True)
 if result.returncode:raise SystemExit(result.returncode)

# Refresh the visual index from the final executable captures.
import json
from PIL import Image,ImageDraw,ImageFont
workspace=max((work/'workspace').glob('run-*/summary.json'),key=lambda p:p.stat().st_mtime)
data=json.loads(workspace.read_text(encoding='utf-8'))
canvas=Image.new('RGB',(1440,1200),(20,22,28));draw=ImageDraw.Draw(canvas)
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',18)
for index,row in enumerate(data['cases']):
    image=Image.open(workspace.parent/row['screenshot']).convert('RGB');image.thumbnail((480,270))
    x=(index%3)*480;y=(index//3)*300
    canvas.paste(image,(x,y+30));draw.text((x+8,y+5),row['workflow']+' '+row['case'],font=font,fill=(230,232,237))
canvas.save(work/'workspace-contact.png')
print('Visual index refreshed',flush=True)
