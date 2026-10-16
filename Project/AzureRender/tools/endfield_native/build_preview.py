"""Build a source-bound technical preview; materials and physics remain approximate."""
import argparse
import bpy, sys, json, math, hashlib
from pathlib import Path
from mathutils import Matrix, Vector, Quaternion

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--run-root',type=Path,required=True)
parser.add_argument('--clip',required=True)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
run = args.run_root.resolve(strict=True)
sys.path.insert(0,str(run/'extractor/src/character_runtime'))
from character_geometry import import_character_geometry

name = args.clip
directory=run/'animation-candidates'/name
if directory.resolve().parent != (run/'animation-candidates').resolve():raise ValueError('Invalid candidate path')
if (directory/'preview.blend').exists():raise FileExistsError(directory/'preview.blend')
verification=json.loads((directory/'generic-verification.json').read_text())
for entry in verification['inputs']:
    assert hashlib.sha256(Path(entry['path']).read_bytes()).hexdigest()==entry['sha256']
source=run/'laevat-static'
audit=json.loads((source/'source_audit.json').read_text())
samples=json.loads((directory/'native/transform_samples.json').read_text())
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
objects=import_character_geometry(source/'model.geometry.json')
rig=next(o for o in objects if o.type=='ARMATURE')
by_path={b['source_transform_path']:b for b in rig.pose.bones}
track_paths={x['source_path']:x['track'] for x in verification['tracks']}
hierarchy=audit['transforms']
reflection=Matrix.Diagonal((-1,1,1,1))
def local(p,q,s):return reflection @ Matrix.LocRotScale(Vector(p),Quaternion((q[3],*q[:3])),Vector(s)) @ reflection
base={row['path']:local(row['position'],row['rotation'],row['scale']) for row in hierarchy}
parents={row['path']:row['path'].rsplit('/',1)[0] if '/' in row['path'] else None for row in hierarchy}
ordered=sorted(base,key=lambda p:p.count('/'))
max_error=0.0
for index,frame in enumerate(samples):
    worlds={}
    for path in ordered:
        matrix=base[path]
        if path in track_paths:
            t=frame['tracks'][track_paths[path]]
            matrix=local(t['translation'],t['rotation'],t['scale'])
        worlds[path]=worlds[parents[path]] @ matrix if parents[path] else matrix
    bpy.context.scene.frame_set(index+1)
    for path in ordered:
        bone=by_path.get(path)
        if bone is None:continue
        # Convert source world-space matrices to Blender's rest-relative channels.
        bone.rotation_mode='QUATERNION'
        bone.matrix_basis=rig.data.bones[bone.name].convert_local_to_pose(
            worlds[path],rig.data.bones[bone.name].matrix_local,
            parent_matrix=worlds[parents[path]] if bone.parent else Matrix.Identity(4),
            parent_matrix_local=bone.parent.bone.matrix_local if bone.parent else Matrix.Identity(4),invert=True)
        bone.keyframe_insert('location',frame=index+1)
        bone.keyframe_insert('rotation_quaternion',frame=index+1)
        bone.keyframe_insert('scale',frame=index+1)
    bpy.context.view_layer.update()
    if index in (0,len(samples)//2,len(samples)-1):
        for path,bone in by_path.items():
            max_error=max(max_error,max(abs(bone.matrix[i][j]-worlds[path][i][j]) for i in range(4) for j in range(4)))
assert max_error<1e-4,max_error
texture_rows=json.loads((source/'source_texture_artifacts.json').read_text())
textures={(r['object']['source'],r['object']['path_id']):r for r in texture_rows}
material_rows={r['object']['name']:r for r in audit['materials']}
for mat in bpy.data.materials:
    mat.use_nodes=True
    bsdf=mat.node_tree.nodes.get('Principled BSDF')
    if bsdf is None:continue
    bsdf.inputs['Roughness'].default_value=.7
    row=material_rows.get(mat.name)
    if row:
        slots=[s for s in row['textures'] if s['slot']=='_BaseMap' and s['reference']['resolved']]
        if slots:
            ref=slots[0]['reference']['resolved'];tex=textures.get((ref['source'],ref['path_id']))
            if tex:
                image=bpy.data.images.load(str(source/tex['png']['path']),check_existing=True);image.pack()
                node=mat.node_tree.nodes.new('ShaderNodeTexImage');node.image=image
                mat.node_tree.links.new(node.outputs['Color'],bsdf.inputs['Base Color'])
                mat.node_tree.links.new(node.outputs['Alpha'],bsdf.inputs['Alpha'])
for obj in objects:
    if obj.type=='MESH':
        path=obj.get('source_geometry_path','')
        obj.hide_render='/lod0/' not in path
        obj.hide_set(obj.hide_render)
scene=bpy.context.scene;scene.render.fps=60;scene.frame_start=1;scene.frame_end=len(samples);scene.frame_set(1)
scene.render.engine='CYCLES';scene.cycles.samples=16
scene.render.resolution_x=640;scene.render.resolution_y=800;scene.render.resolution_percentage=100
scene.world.color=(.15,.15,.15)
bpy.ops.object.camera_add(location=(3,-5,2.1));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,1))-camera.location).to_track_quat('-Z','Y').to_euler();scene.camera=camera;camera.data.type='ORTHO';camera.data.ortho_scale=2.35
bpy.ops.object.light_add(type='AREA',location=(2,-3,4));bpy.context.object.data.energy=450;bpy.context.object.data.shape='DISK';bpy.context.object.data.size=4
bpy.ops.wm.save_as_mainfile(filepath=str(directory/'preview.blend'))
for frame in (1,76):
    scene.frame_set(frame);scene.render.filepath=str(directory/f'preview-{frame}.png');bpy.ops.render.render(write_still=True)
(directory/'preview-verification.json').write_text(json.dumps({'status':'technical_preview','source_tracks':len(track_paths),'rig_bones':len(by_path),'unskinned_tracks':sorted(set(track_paths)-set(by_path)),'maximum_source_world_matrix_error':max_error,'materials':'base_color_approximation','frames':len(samples)},indent=2))
