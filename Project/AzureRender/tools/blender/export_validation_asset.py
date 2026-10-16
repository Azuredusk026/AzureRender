"""Run with Blender --background --factory-startup --python this.py -- arguments."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import bpy
from mathutils import Matrix


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--duration', type=float, required=True)
    parser.add_argument('--clip', default='idle')
    parser.add_argument('--all-vertices', action='store_true')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.output.exists(): raise FileExistsError(args.output)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(args.input.resolve()))
    scene = bpy.context.scene; scene.render.fps = 60
    armatures = [o for o in scene.objects if o.type == 'ARMATURE']
    if len(armatures) != 1: raise ValueError('Expected one armature')
    rig = armatures[0]
    # The importer installs the selected first clip as an action.
    if not rig.animation_data or not rig.animation_data.action: raise ValueError('No active imported action')
    action = rig.animation_data.action
    if args.clip not in action.name: raise ValueError('Imported action differs from requested clip: ' + action.name)
    meshes = sorted([o for o in scene.objects if o.type == 'MESH'], key=lambda o:o.name)
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
            'action':action.name,'sampleRate':60,'allVertices':args.all_vertices,'samples':samples}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('x',encoding='utf-8') as f: json.dump(report,f,separators=(',',':') if args.all_vertices else None,indent=None if args.all_vertices else 2)
    print('VALIDATION_REFERENCE '+str(args.output.resolve()),flush=True)


if __name__=='__main__': main()
