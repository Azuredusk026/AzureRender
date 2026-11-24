from pathlib import Path
run=Path(__file__).resolve().parent
for name in ['update_docs','verify','archive','stage']:
    s=(run/(name+'.py')).read_text()
    s=s.replace("run/'regression-pipeline.json'","run/'shader-family-revision/pipeline.json'")
    if name=='verify':
        s=s.replace("assert read(run/'revision-comparison.json')['status']=='passed'", "assert all(read(run/'shader-family-revision'/(cfg+'-program-compatibility.json'))['status']=='passed' for cfg in ['release','debug'])")
    for fragment in ["run/'compatibility-comparison.json'", "run/f'body-comparison-aa", "run/'related-gpu'", "run/'public-gpu'", "run/('related-'+cfg+'.json')", "run/('route-'+cfg)", "run/'native-capture-summary.json'", "run/'native-captures'"]:
        s=s.replace(fragment,fragment.replace('run/',"run/'shader-family-revision'/",1))
    if name=='update_docs':
        s=s.replace('## 材质照明接收 2026-11-01','## 材质照明接收 2026-11-02')
        s=s.replace('material-lighting-reception/body-comparison-aa0.png','material-lighting-reception/shader-family-revision/body-comparison-aa0.png')
        marker="text+=addition;write(p,text)"
        s=s.replace(marker,"text=text.replace('近景、全身区域及操控的整体复核继续执行。','近景、全身区域及操控按下述范围完成复核。');"+marker)
    if name=='archive':
        s=s.replace("groups+=['failed-observer','compatibility-repeat','determinism-captures']", "groups+=['failed-observer','compatibility-repeat','determinism-captures','shader-family-revision','legacy-shader-captures','repeat-isolations','sync-validation']")
        s=s.replace("if p.name.startswith('frame_'):","if p.name.startswith('frame_'):\n                if group in ['legacy-shader-captures','repeat-isolations','sync-validation'] and p.stem not in ['frame_000000','frame_000009']:continue")
    if name=='stage':
        s=s.replace("'src/render/RenderContext.hpp'", "'src/render/RenderContext.hpp','src/render/MaterialPipelinePolicy.hpp','src/scenes/CharacterMaterialSampling.inc','tests/MaterialPipelinePolicyTests.cpp'")
    (run/('family_'+name+'.py')).write_text(s,newline='\n')
print('Prepared closure drivers; execution waits for complete verification and directory-boundary control')
