"""Verify the first rendered frame after UI LUT undo/redo and Play/Stop changes."""
import argparse
import copy
import json
import os
from pathlib import Path
import subprocess
import tempfile
import numpy as np
from PIL import Image
from create_visibility_fixture import surface,component
from test_color_grading_lut_gpu import write_constant_lut


def run(executable,root,expected_validation):
    root.mkdir(parents=True);assets=root/"assets";assets.mkdir()
    surface(assets/"surface.gltf",double_sided=True)
    colors={"a":[.1,.5,.9],"b":[.9,.1,.1],"c":[.1,.9,.1]}
    for name,color in colors.items():write_constant_lut(assets/(name+".azlut"),color,"linear")
    base={"schemaVersion":1,"id":"lut-editor","sceneType":"character",
        "renderSettings":{"platform":False,"background":False,"innerOutline":False,"silhouetteOutline":False,
            "bloomEnabled":False,"antiAliasing":0,"gradeLut":{"schemaVersion":1,"asset":"assets:/a.azlut","exposureMultiplier":1}},
        "resources":[{"id":"surface","asset":"assets:/surface.gltf"}],"nodes":[{"id":"surface","resourceId":"surface"},
            {"id":"camera","components":{"azure.transform":component("azure.transform",{"translation":[0,0,3]}),
                "azure.camera":component("azure.camera",{"enabled":True,"priority":1,"fieldOfView":55})}}]}
    (assets/"scene.azurelevel").write_text(json.dumps(base))
    other=copy.deepcopy(base);other["id"]="lut-other";other["renderSettings"]["gradeLut"]["asset"]="assets:/c.azlut"
    (assets/"other.azurelevel").write_text(json.dumps(other))
    project=root/"project.azureproject"
    project.write_text(json.dumps({"schemaVersion":1,"id":"lut-editor","name":"LUT editor transition fixture",
        "mounts":[{"name":"assets","path":"assets"}],"startupScene":"assets:/scene.azurelevel"}))
    actions=[dict(frame=2,command="edit",operation="viewport.overlay-policy",parameters={"category":name,"policy":"hidden"})
        for name in ("grid","camera","light","skeleton")]
    actions += [dict(frame=3,command="edit",operation="render.settings",parameters={"values":{"gradeLut":{
        "schemaVersion":1,"asset":"assets:/b.azlut","exposureMultiplier":1}}}),
        dict(frame=20,command="level",value="assets:/other.azurelevel"),
        dict(frame=21,command="wait-level",value="assets:/other.azurelevel")]
    (root/"actions.json").write_text(json.dumps(actions))
    ui=[]
    for frame,key in ((8,"Z"),(12,"Y"),(16,"P"),(48,"P")):
        ui += [dict(frame=frame,action="key",key=key,ctrl=True,down=True),
            dict(frame=frame+1,action="key",key=key,ctrl=False,down=False)]
    (root/"ui.json").write_text(json.dumps(ui))
    env=dict(os.environ,AZURERENDER_EDITOR_CONFIG=str(root/"config"),AZURERENDER_EDITOR_UI_ACTIONS=str(root/"ui.json"))
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    command=[str(executable.resolve()),"--editor-project",str(project),"--editor-close-policy","discard",
        "--editor-actions",str(root/"actions.json"),"--width","1280","--height","720","--fixed-frame-step",
        "--capture-dir",str(root/"frames"),"--capture-frames","54","--runtime-report",str(root/"runtime.json"),
        "--set","runtime.maxFrameRate=30"]
    (root/"command.json").write_text(json.dumps(command,indent=2))
    result=subprocess.run(command,cwd=root,env=env,startupinfo=startup,capture_output=True,text=True,encoding="utf8",errors="replace",timeout=180)
    (root/"run.log").write_text(result.stdout+result.stderr)
    assert result.returncode==0,result.stdout[-4000:]+result.stderr[-4000:]
    assert "Validation layer: "+expected_validation in result.stdout
    combined=result.stdout+result.stderr
    assert "VUID-" not in combined and "Validation Error" not in combined,combined[-4000:]
    assert "Allocator after unload: buffers=0 images=0" in result.stdout
    report=json.loads((root/"runtime.json").read_text(encoding="utf8"));workspace=report["editorWorkspace"]
    assert workspace["uiErrors"]==[] and all(action["passed"] for action in report["editorActions"])
    history={row["frame"]:row for row in workspace["history"]}
    assert history[8]["undoCount"]==history[7]["undoCount"]-1,"UI undo was not applied in its scheduled frame"
    assert history[12]["undoCount"]==history[11]["undoCount"]+1,"UI redo was not applied in its scheduled frame"
    assert history[16]["playing"] and not history[15]["playing"],"UI Play transition was not applied"
    assert not history[48]["playing"] and history[47]["playing"],"UI Stop transition was not applied"
    rect=workspace["image"];x=int(rect["x"]+rect["width"]*.65);y=int(rect["y"]+rect["height"]*.6)
    expected_colors={name:np.rint(np.where(np.asarray(color)<=.0031308,np.asarray(color)*12.92,
        1.055*np.asarray(color)**(1/2.4)-.055)*255) for name,color in colors.items()}
    checked=[]
    # The input history records actual GUI frames; each successful GUI frame has one captured frame.
    for frame,name in ((7,"b"),(8,"a"),(9,"a"),(11,"a"),(12,"b"),(13,"b"),(16,"b"),(46,"c"),(47,"c"),(48,"b"),(49,"b")):
        image=np.asarray(Image.open(root/"frames"/f"frame_{frame-1:06d}.png").convert("RGB"),dtype=float)
        error=float(np.abs(image[y:y+8,x:x+8]-expected_colors[name]).max())
        checked.append(dict(uiFrame=frame,lut=name,pixel=image[y,x].tolist(),maximumByteError=error))
    passed=all(row["maximumByteError"]<=1 for row in checked)
    summary=dict(schemaVersion=1,status="passed" if passed else "failed",validationEnabled=expected_validation=="enabled",frames=checked,
        uiUndoRedoFirstFrame=all(row["maximumByteError"]<=1 for row in checked if row["uiFrame"] in (8,12)),
        playStopFirstFrame=all(row["maximumByteError"]<=1 for row in checked if row["uiFrame"] in (16,48)),liveAllocationsAfterUnload=0)
    (root/"summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    assert passed,checked
    print(json.dumps(summary))


if __name__=="__main__":
    parser=argparse.ArgumentParser(__doc__);parser.add_argument("--executable",type=Path,required=True)
    parser.add_argument("--output",type=Path);parser.add_argument("--expected-validation",choices=("enabled","disabled"),default="enabled")
    args=parser.parse_args()
    if args.output:run(args.executable,args.output.resolve(),args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix="lut-editor-",dir=Path(__file__).resolve().parents[1]/"build/runs") as folder:
            run(args.executable,Path(folder)/"evidence",args.expected_validation)
