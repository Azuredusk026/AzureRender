"""Export bounded pre-dispatch inputs from the fingerprinted capture."""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import subprocess
from native_render.ray_fixture import verify_payloads


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path,required=True)
    parser.add_argument('--renderdoc',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    engine=Path(__file__).resolve().parents[1]
    reference=json.loads((engine/'docs/acceptance/nr/evidence/2026-10-31/ray-arithmetic-order/reference.json').read_text())
    if args.capture.stat().st_size!=reference['captureBytes']:raise ValueError('Capture length differs from its reference')
    with args.capture.open('rb') as source:
        if hashlib.file_digest(source,'sha256').hexdigest()!=reference['captureSha256']:raise ValueError('Capture fingerprint differs from its reference')
    output=args.output.resolve();output.mkdir()
    request=output/'request.json'
    request.write_text(json.dumps(dict(engine=str(engine),capture=str(args.capture.resolve()),output=str(output)),indent=2)+'\n',encoding='utf-8')
    environment=os.environ.copy()
    for key in ('VK_LAYER_PATH','VK_INSTANCE_LAYERS','ENABLE_VULKAN_RENDERDOC_CAPTURE'):environment.pop(key,None)
    environment['AZURE_RAY_SNAPSHOT_REQUEST']=str(request)
    worker=engine/'tools/native_render/ray_snapshot_worker.py'
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    command=[str(args.renderdoc.resolve()),'--python',str(worker)]
    report=dict(status='running',command=command,startUtc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        workerSha256=hashlib.sha256(worker.read_bytes()).hexdigest(),renderdocSha256=hashlib.sha256(args.renderdoc.read_bytes()).hexdigest())
    try:
        with (output/'renderdoc.log').open('w',encoding='utf-8') as log:
            process=subprocess.Popen(command,cwd=engine,env=environment,stdout=log,stderr=subprocess.STDOUT,startupinfo=startup)
            report['pid']=process.pid
            (output/'parent.json').write_text(json.dumps(report,indent=2)+'\n')
            try:result=process.wait(timeout=300)
            except subprocess.TimeoutExpired:
                subprocess.run(['taskkill','/PID',str(process.pid),'/T','/F'],stdout=log,stderr=subprocess.STDOUT)
                process.wait();raise
        child=json.loads((output/'snapshot-report.json').read_text(encoding='utf-8'))
        report.update(exitCode=result,child=child)
        if result!=0 or child['status']!='passed':raise RuntimeError(child.get('error','Snapshot failed'))
        manifest=json.loads((output/'manifest.json').read_text(encoding='utf-8'))
        report.update(status='passed',verifiedBytes=verify_payloads(output,manifest))
        print('Snapshot passed; bytes',manifest['bytes'],'tiles',manifest['tiles']['activeCount'])
    except Exception as error:
        report.update(status='failed',error=str(error));raise
    finally:
        report['endUtc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
        (output/'parent.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
