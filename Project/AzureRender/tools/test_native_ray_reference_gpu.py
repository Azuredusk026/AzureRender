"""Run strict Vulkan reference cases in an owned artifact directory."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import subprocess
import sys
from native_render.ray_fixture import verify_payloads

ENGINE=Path(__file__).resolve().parents[1]


def synthetic(root: Path,active: int,wrong_reference=False):
    root.mkdir()
    def payload(name,data):
        (root/name).write_bytes(data)
        return dict(file=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
    constants=[]
    for i,size in enumerate((1040,48,96)):
        data=bytearray(size)
        if i==2:struct.pack_into('<4f',data,0,8,8,.125,.125)
        constants.append(dict(index=i,**payload('constant-{}.bin'.format(i),data)))
    textures=[]
    for binding,(role,fmt,width,height,texel) in enumerate((
        ('previousColor','R16G16B16A16_FLOAT',16,16,8),('previousDepth','R32_FLOAT',8,8,4),
        ('currentDepth','R32_FLOAT',8,8,4),('motion','R10G10B10A2_UNORM',16,16,4),
        ('surfaceMask','R8_UNORM',8,8,1),('surfaceNormal','R10G10B10A2_UNORM',8,8,4)),1):
        data=bytes(width*height*texel)
        textures.append(dict(role=role,binding=binding,format=fmt,width=width,height=height,texelBytes=texel,
            levels=[dict(mip=0,width=width,height=height,offset=0,bytes=len(data))],**payload(role+'.bin',data)))
    outputs=[]
    for binding,(role,fmt,texel,one) in enumerate((
        ('color','R11G11B10_FLOAT',4,struct.pack('<I',0x781e03c0)),
        ('footprint','R16G16_UNORM',4,struct.pack('<HH',65535,32768)),
        ('confidence','R8_UNORM',1,b'\0'))):
        initial=one*64
        expected=bytearray(bytes(len(initial)) if active else initial)
        if wrong_reference and binding==0:expected[0]^=1
        outputs.append(dict(role=role,binding=binding,format=fmt,width=8,height=8,texelBytes=texel,
            initial=payload(role+'-initial.bin',initial),reference=payload(role+'-reference.bin',expected)))
    tiles=dict(stride=4,activeCount=active,**payload('tiles.bin',struct.pack('<I',0)))
    document=dict(schemaVersion=1,status='complete',dispatch=[active,1,1],localSize=[8,8,1],constants=constants,tiles=tiles,textures=textures,outputs=outputs)
    path=root/'manifest.json';path.write_text(json.dumps(document,indent=2)+'\n',encoding='utf-8')
    return path


def malformed_cases(root: Path):
    for name in ('version','dispatch','tile-range','tile-duplicate','constant-size','texture-format','mip-count'):
        path=synthetic(root/name,2 if name=='tile-duplicate' else 1)
        document=json.loads(path.read_text())
        def change_payload(row,data):
            (path.parent/row['file']).write_bytes(data)
            row.update(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
        if name=='version':document['schemaVersion']=2
        elif name=='dispatch':document['dispatch']=[2,1,1]
        elif name=='tile-range':change_payload(document['tiles'],struct.pack('<I',0xffff0000))
        elif name=='tile-duplicate':change_payload(document['tiles'],bytes(8))
        elif name=='constant-size':change_payload(document['constants'][0],bytes(16))
        elif name=='texture-format':document['textures'][0]['format']='R32_FLOAT'
        else:
            row=document['textures'][0];levels=[];data=bytearray()
            for mip in range(7):
                width=max(1,row['width']>>mip);height=max(1,row['height']>>mip)
                size=width*height*row['texelBytes'];levels.append(dict(mip=mip,width=width,height=height,offset=len(data),bytes=size));data.extend(bytes(size))
            row['levels']=levels;change_payload(row,bytes(data))
        path.write_text(json.dumps(document,indent=2)+'\n',encoding='utf-8')
        yield name,path


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--shader',type=Path,required=True)
    parser.add_argument('--fixture',type=Path)
    args=parser.parse_args()
    run=Path(subprocess.check_output([sys.executable,str(ENGINE/'tools/artifact_cleanup.py'),'--new-run','native-ray-reference'],cwd=ENGINE,text=True).strip())
    rows=[]
    if args.fixture:
        cases=[('native',args.fixture.resolve(),False,True),('native-reversed',args.fixture.resolve(),True,True)]
    else:
        cases=[('masked',synthetic(run/'masked',1),False,True),('zero-dispatch',synthetic(run/'zero-dispatch',0),False,True),
            ('negative-one-byte',synthetic(run/'negative-one-byte',1,True),False,False)]
    report=dict(status='running',shaderSha256=hashlib.sha256(args.shader.read_bytes()).hexdigest(),cases=rows)
    try:
        for name,fixture,reverse,success in cases:
            document=json.loads(fixture.read_text(encoding='utf-8'))
            verify_payloads(fixture.parent,document)
            output=run/(name+'-output')
            command=[str(args.executable.resolve()),str(args.shader.resolve()),str(fixture),str(output)]
            if reverse:command.append('--reverse')
            result=subprocess.run(command,cwd=ENGINE,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=120)
            (run/(name+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
            child=json.loads((output/'result.json').read_text(encoding='utf-8'))
            verify_payloads(fixture.parent,document)
            row=dict(name=name,exitCode=result.returncode,command=command,result=child);rows.append(row)
            if success:
                assert result.returncode==0 and child['status']=='passed',row
                assert all(item['differentBytes']==0 for item in child['outputs'])
            else:
                assert result.returncode!=0 and child['outputs'][0]['differentBytes']==1,row
            assert child['validationErrors']==child['liveBuffers']==child['liveImages']==0,row
        if not args.fixture:
            for name,fixture in malformed_cases(run):
                document=json.loads(fixture.read_text(encoding='utf-8'));verify_payloads(fixture.parent,document) if name!='version' else None
                output=run/(name+'-output')
                command=[str(args.executable.resolve()),str(args.shader.resolve()),str(fixture),str(output)]
                result=subprocess.run(command,cwd=ENGINE,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=30)
                (run/(name+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
                assert result.returncode!=0 and not output.exists(),(name,result.stdout,result.stderr)
                rows.append(dict(name=name,exitCode=result.returncode,rejectedBeforeGpu=True,diagnostic=result.stderr.strip()))
        report['status']='passed'
        subprocess.run([sys.executable,str(ENGINE/'tools/artifact_cleanup.py'),'--finish',str(run)],check=True)
        print('Native ray reference cases passed:',len(rows),str(run))
    except Exception as error:
        report.update(status='failed',error=str(error));raise
    finally:(run/'summary.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
