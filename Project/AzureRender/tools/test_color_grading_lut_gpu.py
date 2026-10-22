"""Exercise public postprocess LUT assets, diagnostic bypass and fallback in Player."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import numpy as np
from PIL import Image
from create_visibility_fixture import surface,component


def write_constant_lut(path,color,domain):
    header={"schemaVersion":1,"dimension":2,"width":4,"height":2,"channels":4,
        "inputDomain":domain,"outputDomain":"linear","layout":"strip-rgb","precision":"float32-le",
        "source":{"generator":"azure.public-lut-gpu-fixture","generatorVersion":1,
            "inputFingerprint":"constant-rgb-contract-v1","evidence":0}}
    encoded=json.dumps(header,separators=(",",":")).encode()
    pixels=np.tile(np.array([*color,1],dtype="<f4"),(2,4,1))
    path.write_bytes(b"AZLUT001"+struct.pack("<I",len(encoded))+encoded+pixels.tobytes())


def run(player,root,expected_validation="enabled"):
    root.mkdir(parents=True,exist_ok=True);assets=root/"assets";assets.mkdir(exist_ok=True)
    surface(assets/"surface.gltf",double_sided=True)
    target=np.array([.1,.5,.9])
    for domain in ("linear","logc3-full-log"):
        write_constant_lut(assets/(domain+".azlut"),target,domain)
    base={"schemaVersion":1,"id":"public-color-grading-lut","sceneType":"character",
        "renderSettings":{"platform":False,"innerOutline":False,"silhouetteOutline":False,"bloomEnabled":False,
            "antiAliasing":0,"exposure":0,"saturation":1,"contrast":1,"toneMapping":True},
        "resources":[{"id":"surface","asset":"assets:/surface.gltf"}],
        "nodes":[{"id":"surface","resourceId":"surface"},{"id":"camera","components":{
            "azure.transform":component("azure.transform",{"translation":[0,0,3]}),
            "azure.camera":component("azure.camera",{"enabled":True,"priority":1,"fieldOfView":55})}}]}
    project=root/"project.azureproject"
    project.write_text(json.dumps({"schemaVersion":1,"id":"public-color-grading-lut","name":"Public color grading fixture",
        "mounts":[{"name":"assets","path":"assets"}],"startupScene":"assets:/scene.azurelevel"}))
    frames={};cases={}
    for name in ("default","linear","logc3-full-log","disabled","diagnostic","fixed","component-diagnostic"):
        level=copy.deepcopy(base)
        if name!="default":
            level["renderSettings"]["gradeLut"]={"schemaVersion":1,"asset":"" if name=="disabled" else "assets:/"+("logc3-full-log" if name=="logc3-full-log" else "linear")+".azlut","exposureMultiplier":1.7}
        if name=="diagnostic":level["renderSettings"]["diagnosticView"]=1
        (assets/"scene.azurelevel").write_text(json.dumps(level))
        folder=root/name;folder.mkdir(exist_ok=True)
        command=[str(player.resolve()),"--project",str(project.resolve()),"--width","128","--height","128",
            "--capture-dir",str(folder/"frames"),"--capture-frames","1","--fixed-frame-step","--qa-camera","scene","--qa-light","scene",
            "--runtime-report",str(folder/"runtime.json")]
        if name=="fixed":command.extend(["--disable-bindless","--disable-compute-skinning","--disable-parallel-recording"])
        if name=="diagnostic":command.extend(["--qa-isolation","world-normal"])
        if name=="component-diagnostic":command.extend(["--qa-isolation","hair-kk"])
        (folder/"command.json").write_text(json.dumps(command,indent=2))
        with (folder/"player.log").open("w",encoding="utf8") as log:
            subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=90)
        text=(folder/"player.log").read_text(encoding="utf8",errors="replace")
        assert "Validation layer: "+expected_validation in text,name
        assert not re.search(r"VUID-|Validation Error|\[error\]",text),name
        assert "Allocator after unload: buffers=0 images=0" in text,name
        report=json.loads((folder/"runtime.json").read_text())
        assert not report["scriptErrors"] and not report["presentationErrors"],name
        manifest=json.loads((folder/"frames/capture_manifest.json").read_text())
        assert manifest["diagnosticView"]==("world-normal" if name=="diagnostic" else "beauty"),name
        lut_manifest=manifest["gradeSettings"]["lut"]
        if name!="component-diagnostic":assert lut_manifest["dimension"]==(0 if name in ("default","disabled") else 2),name
        if lut_manifest["dimension"]:
            assert lut_manifest["exposureMultiplier"]==1.7 and lut_manifest["fingerprint"],name
        frame=np.asarray(Image.open(folder/"frames/frame_000000.png").convert("RGB"),dtype=float)
        frames[name]=frame
        cases[name]={"pixel":frame[64,64].tolist(),"validationEnabled":expected_validation=="enabled",
            "validationErrorsDetected":0,"liveAllocationsAfterUnload":0}
        if name=="component-diagnostic":
            # Project the fixture's half-metre extent at z=0 through its
            # 55-degree camera at z=3. Exclude raster edges and the sky.
            radius=int(64*.5/(3*np.tan(np.deg2rad(55)*.5)))-1
            assert np.max(frame[64-radius:64+radius,64-radius:64+radius])==0,(name,'Component isolation must bypass grading',frame[64,64].tolist())
            assert lut_manifest['dimension']==0 and not manifest['gradeSettings']['toneMappingEnabled'],name
        if name in ("linear","logc3-full-log","fixed"):
            expected=np.rint(np.where(target<=.0031308,target*12.92,1.055*target**(1/2.4)-.055)*255)
            error=np.abs(frame[32:96,32:96]-expected).max()
            assert error<=1,(name,error,frame[64,64].tolist(),expected.tolist())
            cases[name].update(expected=expected.tolist(),maximumByteError=float(error))
    assert np.array_equal(frames["default"],frames["disabled"]),"Disabled LUT must use the default production grade path"
    assert np.array_equal(frames["linear"],frames["logc3-full-log"]),"Constant cube output must be independent of input domain"
    assert np.array_equal(frames["linear"],frames["fixed"]),"Fixed descriptor production path must consume the same LUT"
    assert not np.array_equal(frames["linear"],frames["diagnostic"]),"Diagnostic view must bypass color grading"
    summary={"schemaVersion":1,"playerSha256":hashlib.sha256(player.read_bytes()).hexdigest(),"cases":cases,
        "scope":"Production LDR lookup, output encoding, default fallback, diagnostic bypass and unload; linear HDR 1% acceptance remains separate",
        "byteTolerance":1,"validationEnabled":expected_validation=="enabled","validationErrorsDetected":0}
    (root/"summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(summary))


if __name__=="__main__":
    parser=argparse.ArgumentParser(__doc__);parser.add_argument("--executable",type=Path,required=True);parser.add_argument("--output",type=Path)
    parser.add_argument("--expected-validation",choices=("enabled","disabled"),default="enabled")
    args=parser.parse_args()
    if args.output:run(args.executable,args.output.resolve(),args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix="public-lut-",dir=Path(__file__).resolve().parents[1]/"build/runs") as root:
            run(args.executable,Path(root),args.expected_validation)
