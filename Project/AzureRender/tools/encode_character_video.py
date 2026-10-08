"""Encode the fixed 1440p character delivery and independently verify MP4 headers."""
import argparse,json,subprocess,hashlib,struct,re
from pathlib import Path
import imageio_ffmpeg

def boxes(data):
    offset=0
    while offset+8<=len(data):
        size,name=struct.unpack_from('>I4s',data,offset);header=8
        if size==1:size=struct.unpack_from('>Q',data,offset+8)[0];header=16
        if size==0:size=len(data)-offset
        if size<header or offset+size>len(data):raise ValueError('Invalid MP4 box')
        yield name,data[offset+header:offset+size];offset+=size

def find(data,name):return next(content for kind,content in boxes(data) if kind==name)
def verify(path):
    moov=find(path.read_bytes(),b'moov');tracks=[v for k,v in boxes(moov) if k==b'trak'];assert len(tracks)==1
    mdia=find(tracks[0],b'mdia');handler=find(mdia,b'hdlr');assert handler[8:12]==b'vide'
    mdhd=find(mdia,b'mdhd');assert mdhd[0]==0
    timescale,duration=struct.unpack_from('>II',mdhd,12);assert duration/timescale==16
    stbl=find(find(mdia,b'minf'),b'stbl');count=struct.unpack_from('>I',find(stbl,b'stsz'),8)[0];assert count==384
    stsd=find(stbl,b'stsd');entry=find(stsd[8:],b'avc1');width,height=struct.unpack_from('>HH',entry,24);assert [width,height]==[2560,1440]
    avcc=find(entry[78:],b'avcC');assert avcc[1]==100,'H.264 High profile required'
    length=struct.unpack_from('>H',avcc,6)[0];sps=avcc[8:8+length]
    rbsp=sps.replace(b'\0\0\3',b'\0\0');bits=''.join(f'{b:08b}' for b in rbsp[4:]);position=0
    def ue():
        nonlocal position
        zeros=0
        while bits[position]=='0':zeros+=1;position+=1
        value=int(bits[position:position+zeros+1],2)-1;position+=zeros+1;return value
    ue();assert ue()==1 and ue()==0 and ue()==0,'8-bit YUV 4:2:0 SPS required'
    colr=find(entry[78:],b'colr');assert colr[:4]==b'nclx' and struct.unpack_from('>HHH',colr,4)==(1,1,1)
    return {'width':width,'height':height,'frames':count,'durationSeconds':duration/timescale,'fps':count/(duration/timescale),
            'profileIdc':avcc[1],'colorPrimaries':1,'colorTransfer':1,'colorMatrix':1,'tracks':1,'audio':False}

def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('--frames',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();frames=args.frames.resolve();output=args.output.resolve();output.parent.mkdir(parents=True,exist_ok=True)
    assert len(list(frames.glob('frame_*.png')))==384
    ffmpeg=imageio_ffmpeg.get_ffmpeg_exe()
    command=[ffmpeg,'-hide_banner','-nostdin','-n','-framerate','24','-i',str(frames/'frame_%06d.png'),'-frames:v','384',
             '-vf','scale=iw:ih:in_range=pc:out_range=tv:out_color_matrix=bt709,setparams=color_primaries=bt709:color_trc=bt709:colorspace=bt709:range=limited','-c:v','libx264','-profile:v','high',
             '-preset','slow','-crf','15','-pix_fmt','yuv420p','-color_primaries','bt709','-color_trc','bt709','-colorspace','bt709',
             '-color_range','tv','-an','-movflags','+faststart+write_colr',str(output)]
    run=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace');output.with_suffix('.encode.log').write_text(run.stderr,encoding='utf-8');assert run.returncode==0,run.stderr[-4000:]
    report=verify(output)
    decode=subprocess.run([ffmpeg,'-hide_banner','-nostdin','-v','error','-i',str(output),'-map','0:v:0','-progress','pipe:1','-f','null','-'],capture_output=True,text=True,encoding='utf-8',errors='replace')
    output.with_suffix('.decode.log').write_text(decode.stdout+decode.stderr,encoding='utf-8')
    counts=re.findall(r'^frame=(\d+)$',decode.stdout,re.M);assert decode.returncode==0 and not decode.stderr.strip() and int(counts[-1])==384
    report.update(codec='H.264 High',pixelFormat='yuv420p',colorSpace='BT.709',crf=15,preset='slow',fullDecodeVerified=True,
                  video=str(output),videoBytes=output.stat().st_size,videoSha256=hashlib.sha256(output.read_bytes()).hexdigest(),encoder=ffmpeg,
                  captureManifest=str(frames/'capture_manifest.json'))
    output.with_suffix('.delivery.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report))
if __name__=='__main__':main()
