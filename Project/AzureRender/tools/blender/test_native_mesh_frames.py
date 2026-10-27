"""Check authored corner normals through the native OBJ coordinate conversion."""
import argparse
import json
from pathlib import Path
import sys

import bpy
import numpy as np

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--extractor-src',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--adapter',choices=('native','upstream'),default='native')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
engine=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(engine/'tools/blender'),str(engine/'tools'),str(args.extractor_src)]
if args.adapter=='upstream':
    from blender_build_map02_chunk_20260727 import import_obj
else:
    from native_mesh_frames import import_native_obj as import_obj
args.output.mkdir(parents=True,exist_ok=False)
points=[[-1,0,-1],[1,0,-1],[1,0,1],[-1,0,1]]
normals=np.asarray([[.35,.92,.15],[-.25,.9,.3]],dtype=float)
normals/=np.linalg.norm(normals,axis=1,keepdims=True)
lines=['v '+str(-x)+' '+str(y)+' '+str(z) for x,y,z in points]
lines+=['vt 0 0','vt 1 0','vt 1 1','vt 0 1']
lines+=['vn '+str(-x)+' '+str(y)+' '+str(z) for x,y,z in normals]
lines+=['g split_0','f 1/1/1 2/2/1 3/3/1','g split_1','f 1/1/2 3/3/2 4/4/2']
source=args.output/'authored.obj';source.write_text('\n'.join(lines)+'\n')
objects=import_obj(source);assert len(objects)==2
maximum=0.;rows=[]
for obj in objects:
    mesh=obj.data;expected=normals[int(obj.name.rsplit('_',1)[-1])]
    expected=np.asarray([expected[0],-expected[2],expected[1]])
    actual=np.asarray([list(n.vector) for n in mesh.corner_normals])
    # Calibrate Blender's custom-normal storage independently of OBJ conversion.
    control=mesh.copy();control.normals_split_custom_set([expected.tolist()]*len(control.loops));control.update()
    represented=np.asarray([list(n.vector) for n in control.corner_normals])
    error=float(np.max(np.abs(actual-represented)))
    storage_error=float(np.max(np.abs(represented-expected)));bpy.data.meshes.remove(control)
    maximum=max(maximum,error);rows.append(dict(mesh=obj.name,expected=expected.tolist(),actual=actual.tolist(),maximumError=error,customNormals=mesh.has_custom_normals,blenderStorageError=storage_error))
extra={}
if args.adapter=='native':
    original=source.read_text()
    malformed=[original.replace('1/1/1','1/1',1),original.replace('1/1/1','99/1/1',1),
        original.replace(next(line for line in lines if line.startswith('vn ')),'vn 0 0 0',1),
        original.replace(next(line for line in lines if line.startswith('vn ')),'vn nan 1 0',1),
        original.replace('f 1/1/1 2/2/1 3/3/1','f 1/1/1 2/2/1 3/3/1 4/4/1')]
    for index,text in enumerate(malformed):
        bad=args.output/f'invalid-{index}.obj';bad.write_text(text)
        before=(len(bpy.data.objects),len(bpy.data.meshes))
        try:import_obj(bad)
        except ValueError:pass
        else:raise AssertionError('Malformed authored normal input accepted')
        assert before==(len(bpy.data.objects),len(bpy.data.meshes))
    missing=args.output/'generated.obj';missing.write_text('\n'.join(line for line in lines if not line.startswith('vn ')).replace('/1/1','/1').replace('/2/1','/2').replace('/3/1','/3').replace('/1/2','/1').replace('/3/2','/3').replace('/4/2','/4')+'\n')
    generated=import_obj(missing)
    assert all(not json.loads(obj.data['azureNativeNormalReceipt'])['authoredNormals'] for obj in generated)
    no_uv=args.output/'no-uv.obj';no_uv.write_text('\n'.join(line for line in lines if not line.startswith('vt ')).replace('/1/','//').replace('/2/','//').replace('/3/','//').replace('/4/','//')+'\n')
    independent=import_obj(no_uv)
    assert all(json.loads(obj.data['azureNativeNormalReceipt'])['authoredNormals'] for obj in independent)
    for obj in independent:
        expected=normals[int(obj.name.split('.')[0].rsplit('_',1)[-1])]
        expected=np.asarray([expected[0],-expected[2],expected[1]])
        assert max(np.linalg.norm(np.asarray(n.vector)-expected) for n in obj.data.corner_normals)<1e-4
    extra=dict(invalidInputs=len(malformed),generatedNormals=True,authoredNormalsWithoutUv=True)
result=dict(status='passed' if maximum<2e-5 else 'failed',maximumError=maximum,budget=2e-5,meshes=rows,
            flipNormalsDocumentation=objects[0].data.flip_normals.__doc__,extra=extra)
(args.output/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
assert result['status']=='passed',json.dumps(result)
print('Authored corner normals preserved:',len(objects),'meshes')
