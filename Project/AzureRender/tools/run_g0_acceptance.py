"""Compare the frozen rendering installation with both G0 hosts."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]

def run(exe,args,cwd,log):
    result=subprocess.run([str(exe),*map(str,args)],cwd=cwd,capture_output=True,encoding="utf-8",errors="replace",timeout=180)
    text=result.stdout+result.stderr;log.write_text(text,encoding="utf-8")
    if result.returncode or "VUID-" in text or "Validation Error" in text:
        raise RuntimeError(f"Runtime failed: {log}")
    return text

def main():
    parser=argparse.ArgumentParser()
    for name in ["baseline-executable","executable","player","build-dir","output-dir"]:
        parser.add_argument("--"+name,required=True,type=Path)
    parser.add_argument("--previous-results",type=Path)
    args=parser.parse_args();output=args.output_dir.resolve();output.mkdir(parents=True,exist_ok=False)
    report={"status":"active","images":{},"cpu":{}}
    previous=None
    if args.previous_results:
        previous=json.loads(args.previous_results.read_text());assert previous["status"]=="passed"
        report["referenceEvidence"]={"path":str(args.previous_results),"sha256":hashlib.sha256(args.previous_results.read_bytes()).hexdigest()}
    try:
        ninja=(args.build_dir/"build.ninja").read_text(encoding="utf-8")
        link=next(line for line in ninja.splitlines() if line.startswith("build AzurePlayer.exe:"))
        if "AzureEditor" in link or "imgui" in link.lower(): raise ValueError("Player links editor dependencies")
        (output/"player-link.txt").write_text(link+"\n",encoding="utf-8")
        player=args.player.resolve();project=output/"project"
        run(player,["--create-project",project],output,output/"create-project.log")
        projectFile=project/"project.azureproject"
        for scene in ["sample","character","blackhole"]:
            hashes={}
            for label,exe in [("baseline",args.baseline_executable.resolve()),("render",args.executable.resolve()),("player",player)]:
                if label=="baseline" and previous:
                    hashes[label]=previous["images"][scene]["hashes"]["baseline"]
                    continue
                captures=output/f"{scene}-{label}"
                flags=["--scene-type",scene,"--width","1280","--height","720","--capture-dir",captures,"--capture-frames","4","--capture-fps","60"]
                if scene=="blackhole":flags += ["--blackhole-quality","cinematic","--blackhole-camera","front"]
                if label=="player":flags += ["--project",projectFile]
                text=run(exe,flags,output,output/f"{scene}-{label}.log")
                if "Allocator after unload: buffers=0 images=0" not in text:raise ValueError("Resources not released")
                images=sorted(captures.glob("frame_*.png"))
                if len(images)!=4:raise ValueError("Incomplete captures")
                hashes[label]=[]
                for imageFile in images:
                    with Image.open(imageFile) as image:
                        image.load();hashes[label].append(hashlib.sha256(image.convert("RGBA").tobytes()).hexdigest())
            if not hashes["baseline"]==hashes["render"]==hashes["player"]:raise ValueError(f"Image mismatch: {scene}")
            report["images"][scene]={"frames":4,"hashes":hashes,"passed":True}
            print(f"{scene}: baseline, AzureRender and AzurePlayer pixels match",flush=True)
        # Same machine, same low workload, repeated runs distinguish scheduler cost.
        for label,exe,extra in [("baseline",args.baseline_executable.resolve(),[]),("adaptive",args.executable.resolve(),[]),("serial",args.executable.resolve(),["--disable-parallel-recording"])]:
            if label!="adaptive" and previous:
                report["cpu"][label]=previous["cpu"][label]
                continue
            values=[]
            for repeat in range(3):
                timing=output/f"cpu-{label}-{repeat}.json"
                run(exe,["--scene-type","character","--smoke-frames","300","--fixed-frame-step","--gpu-timing","--gpu-timing-output",timing,*extra],output,output/f"cpu-{label}-{repeat}.log")
                data=json.loads(timing.read_text());submission=data["submission"]
                if data["samples"]!=300 or submission["frames"]!=300:raise ValueError("Incomplete CPU measurement")
                values.append(submission["recordingMilliseconds"]/300)
            report["cpu"][label]={"averagesMs":values}
        report["status"]="passed"
    except Exception as error:
        report.update(status="failed",error=str(error));raise
    finally:(output/"summary.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")

if __name__=="__main__":main()
