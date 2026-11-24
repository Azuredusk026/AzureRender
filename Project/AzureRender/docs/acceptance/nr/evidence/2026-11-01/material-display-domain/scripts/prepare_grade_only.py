import copy,hashlib,json,os,shutil
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
source=root/'build/runs/20261101-021345-material-radiance-normalization-1e6231/project';project=run/'project'
assert not project.exists();project.mkdir();linked=copied=0
immutable={'.glb','.bin','.dds','.ktx2','.png','.jpg','.jpeg','.exr','.azic','.azlut','.azfog','.hdr','.fbx'}
for directory,dirs,files in os.walk(source):
    dirs[:]=[name for name in dirs if name!='.azure'];relative=Path(directory).relative_to(source);target=project/relative;target.mkdir(parents=True,exist_ok=True)
    for name in files:
        src=Path(directory)/name;dst=target/name
        assert src.resolve().is_relative_to(source.resolve()) and dst.resolve().is_relative_to(project.resolve())
        if src.suffix.lower() in immutable:os.link(src,dst);linked+=1
        else:shutil.copy2(src,dst);copied+=1
base=json.loads((project/'scene/near-normalized-isolated.azurelevel').read_text());world=json.loads((project/'scene/near-world-normalized.azurelevel').read_text());graded=copy.deepcopy(base)
graded['renderSettings']['gradeLut']=copy.deepcopy(world['renderSettings']['gradeLut'])
assert graded!=base
restored=copy.deepcopy(graded);restored['renderSettings']['gradeLut']=copy.deepcopy(base['renderSettings']['gradeLut']);assert restored==base
level=project/'scene/near-grade-only.azurelevel';level.write_text(json.dumps(graded,indent=2)+'\n')
descriptor=json.loads((project/'near-normalized-isolated.azureproject').read_text());descriptor.update(id='near-grade-only',name='near-grade-only',startupScene='stage:/near-grade-only.azurelevel')
entry=project/'near-grade-only.azureproject';entry.write_text(json.dumps(descriptor,indent=2)+'\n')
inputs=[entry,level,project/'content-character/hero-restored-inputs-v8.glb',project/'native-gi/captured-3534.azlut']
inputs += [p for p in (project/'assets/normalized-character-materials').iterdir() if p.is_file()]
report={'status':'prepared','project':str(entry),'level':str(level),'onlyRenderDifference':'renderSettings.gradeLut','controlLevelSha256':hashlib.sha256((project/'scene/near-normalized-isolated.azurelevel').read_bytes()).hexdigest(),
    'linkedFiles':linked,'copiedFiles':copied,'sourceProject':str(source),'inputs':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}}
(run/'grade-only-inputs.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'linkedFiles':linked,'copiedFiles':copied,'onlyRenderDifference':report['onlyRenderDifference']}))
