"""Run with Blender --background --factory-startup --python this.py -- arguments."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Matrix


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--duration', type=float, required=True)
    parser.add_argument('--clip', default='idle')
    parser.add_argument('--all-vertices', action='store_true')
    parser.add_argument('--prepared-inspection', type=Path)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.output.exists(): raise FileExistsError(args.output)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.render.fps = 60
    bpy.ops.import_scene.gltf(filepath=str(args.input.resolve()))
    scene = bpy.context.scene; scene.render.fps = 60
    armatures = [o for o in scene.objects if o.type == 'ARMATURE']
    if len(armatures) != 1: raise ValueError('Expected one armature')
    rig = armatures[0]
    # The importer installs the selected first clip as an action.
    if not rig.animation_data or not rig.animation_data.action: raise ValueError('No active imported action')
    action = rig.animation_data.action
    if args.clip not in action.name: raise ValueError('Imported action differs from requested clip: ' + action.name)
    meshes = sorted([o for o in scene.objects if o.type == 'MESH' and any(m.type=='ARMATURE' for m in o.modifiers)], key=lambda o:o.name)
    if args.prepared_inspection:
        sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
        from validation_asset import read_glb, accessor
        document,chunks=read_glb(args.input);binary=next(data for kind,data in chunks if kind==0x004e4942)
        original=np.concatenate([accessor(document,binary,p['attributes']['POSITION']) for m in document['meshes'] for p in m['primitives']])
        prepared=np.asarray(json.loads(args.prepared_inspection.read_text())['preparedPositions'])
        if len(prepared)!=sum(len(o.data.vertices) for o in meshes):raise ValueError('Prepared geometry topology mismatch')
        offset=0
        for obj in meshes:
            count=len(obj.data.vertices);raw=original[offset:offset+count]
            imported=np.asarray([list(v.co) for v in obj.data.vertices]);homogeneous=np.c_[raw,np.ones(count)]
            transform,_,rank,_=np.linalg.lstsq(homogeneous,imported,rcond=None)
            if rank!=4 or np.max(np.linalg.norm(homogeneous@transform-imported,axis=1))>1e-5:raise ValueError('Prepared geometry correspondence mismatch')
            corrected=imported+(prepared[offset:offset+count]-raw)@transform[:3]
            for vertex,value in zip(obj.data.vertices,corrected):vertex.co=value
            offset+=count
    times = sorted(set([args.duration*i/4 for i in range(5)] +
                       [min(args.duration,args.duration*i/4+1/120) for i in range(5)]))
    # Blender Z-up to glTF Y-up; transform matrices by basis conjugation.
    basis = Matrix(((1,0,0,0),(0,0,1,0),(0,-1,0,0),(0,0,0,1)))
    inverse = basis.inverted(); samples=[]
    for time in times:
        frame = time*60
        scene.frame_set(math.floor(frame), subframe=frame-math.floor(frame))
        graph=bpy.context.evaluated_depsgraph_get(); graph.update()
        evaluated=rig.evaluated_get(graph)
        bones={b.name:[list(row) for row in basis @ evaluated.matrix_world @ b.matrix @ inverse] for b in evaluated.pose.bones}
        vertices={}
        for obj in meshes:
            eo=obj.evaluated_get(graph); mesh=eo.to_mesh()
            try:
                indices=range(len(mesh.vertices)) if args.all_vertices else sorted(set([0,len(mesh.vertices)//4,len(mesh.vertices)//2,3*len(mesh.vertices)//4,len(mesh.vertices)-1]))
                for index in indices:
                    if index>=0: vertices[obj.name+':'+str(index)]=list(basis @ eo.matrix_world @ mesh.vertices[index].co)
            finally: eo.to_mesh_clear()
        samples.append({'time':time,'bones':bones,'vertices':vertices})
    report={'schemaVersion':1,'space':'gltf-world','units':'metres',
            'transformOrder':'morph, skin, object world, Blender-to-glTF basis',
            'matrixStorage':'row-major','blenderVersion':bpy.app.version_string,
            'sourceSha256':hashlib.sha256(args.input.read_bytes()).hexdigest(),
            'action':action.name,'sampleRate':60,'allVertices':args.all_vertices,
            'enginePreparedGeometry':args.prepared_inspection is not None,'samples':samples}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('x',encoding='utf-8') as f: json.dump(report,f,separators=(',',':') if args.all_vertices else None,indent=None if args.all_vertices else 2)
    print('VALIDATION_REFERENCE '+str(args.output.resolve()),flush=True)


if __name__=='__main__': main()
