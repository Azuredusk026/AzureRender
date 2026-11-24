"""Create authored expression candidates and deformation evidence from explicit world deltas."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Matrix, Vector
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from validation_asset import read_glb, accessor, resample
from retarget_animation import world_matrices


def main():
    p=argparse.ArgumentParser(__doc__)
    for name in ('input','config','inspection','output'):p.add_argument('--'+name,type=Path,required=True)
    args=p.parse_args(sys.argv[sys.argv.index('--')+1:])
    if args.output.exists():raise FileExistsError(args.output)
    config=json.loads(args.config.read_text(encoding='utf-8'));inspection=json.loads(args.inspection.read_text(encoding='utf-8'))
    doc,chunks=read_glb(args.input);nodes=doc['nodes'];parents=[-1]*len(nodes)
    for i,n in enumerate(nodes):
        for child in n.get('children',[]):parents[child]=i
    transforms=[[n.get('translation',[0,0,0]),n.get('rotation',[0,0,0,1]),n.get('scale',[1,1,1])] for n in nodes]
    binary=next(data for kind,data in chunks if kind==0x004e4942)
    animation=doc['animations'][0]
    for channel in animation['channels']:
        sampler=animation['samplers'][channel['sampler']];target=channel['target'];path=target['path']
        if path in ('translation','rotation','scale'):
            transforms[target['node']][{'translation':0,'rotation':1,'scale':2}[path]]=resample(accessor(doc,binary,sampler['input']).flatten(),accessor(doc,binary,sampler['output']),[0],rotation=path=='rotation',interpolation=sampler.get('interpolation','LINEAR'))[0].tolist()
    matrices=world_matrices(transforms,parents)
    paths={n['name']:n['path'] for n in inspection['nodes']};indices={n.get('name'):i for i,n in enumerate(nodes)}
    bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.gltf(filepath=str(args.input.resolve()))
    scene=bpy.context.scene;rig=next(o for o in scene.objects if o.type=='ARMATURE')
    scene.frame_set(0);bpy.context.view_layer.update();original={b.name:b.matrix_basis.copy() for b in rig.pose.bones};rig.animation_data_clear()
    for bone in rig.pose.bones:bone.matrix_basis=original[bone.name]
    bpy.context.view_layer.update()
    meshes=sorted([o for o in scene.objects if o.type=='MESH' and any(m.type=='ARMATURE' for m in o.modifiers)],key=lambda o:o.name)
    influence_counts={}
    for obj in meshes:
        group_names={g.index:g.name for g in obj.vertex_groups}
        for vertex in obj.data.vertices:
            for group in vertex.groups:
                if group.weight>0:
                    name=group_names[group.group];influence_counts[name]=influence_counts.get(name,0)+1
    basis=Matrix(((1,0,0,0),(0,0,1,0),(0,-1,0,0),(0,0,0,1)));inverse=basis.inverted()
    prepared=inspection.get('preparedPositions')
    if prepared is not None:
        original_positions=np.concatenate([accessor(doc,binary,primitive['attributes']['POSITION']) for mesh in doc['meshes'] for primitive in mesh['primitives']])
        if len(prepared)!=sum(len(obj.data.vertices) for obj in meshes):raise ValueError('Prepared geometry topology mismatch')
        offset=0
        for obj in meshes:
            count=len(obj.data.vertices);raw=original_positions[offset:offset+count]
            imported=np.asarray([list(v.co) for v in obj.data.vertices]);homogeneous=np.c_[raw,np.ones(count)]
            transform,_,rank,_=np.linalg.lstsq(homogeneous,imported,rcond=None)
            if rank!=4 or np.max(np.linalg.norm(homogeneous@transform-imported,axis=1))>1e-5:
                raise ValueError('Prepared geometry vertex correspondence is not an affine import transform')
            corrected=imported+(np.asarray(prepared[offset:offset+count])-raw)@transform[:3]
            for vertex,value in zip(obj.data.vertices,corrected):vertex.co=Vector(value)
            offset+=count
        bpy.context.view_layer.update()
    def vertices():
        graph=bpy.context.evaluated_depsgraph_get();graph.update();parts=[]
        for obj in meshes:
            evaluated=obj.evaluated_get(graph);mesh=evaluated.to_mesh()
            try:parts.extend([list(basis@evaluated.matrix_world@v.co) for v in mesh.vertices])
            finally:evaluated.to_mesh_clear()
        return np.asarray(parts)
    base=vertices();rest={b.name:b.matrix.copy() for b in rig.pose.bones}
    camera_data=bpy.data.cameras.new('ExpressionCamera');camera=bpy.data.objects.new('ExpressionCamera',camera_data);scene.collection.objects.link(camera)
    target=inverse@Vector(config['cameraTarget']);camera.location=inverse@Vector(config['cameraPosition'])
    if config.get('cameraAnchor'):
        target=rig.matrix_world@rig.pose.bones[config['cameraAnchor']].matrix.translation
        camera.location=target+inverse.to_3x3()@Vector([0,0,.7])
    camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera_data.type='ORTHO';camera_data.ortho_scale=config.get('cameraScale',.25);scene.camera=camera
    scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL'
    if scene.world is None:scene.world=bpy.data.worlds.new('ExpressionWorld')
    scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True;scene.display.shading.background_type='WORLD';scene.world.color=(.07,.07,.07)
    scene.render.resolution_x=768;scene.render.resolution_y=768;scene.render.resolution_percentage=100
    args.output.mkdir(parents=True);reports=[]
    for preset in config['presets']:
        for bone in rig.pose.bones:bone.matrix_basis=original[bone.name]
        bpy.context.view_layer.update();deltas=[];influences={}
        for name,delta in preset['worldTranslations'].items():
            index=indices[name];parent=parents[index];world=np.asarray(delta,dtype=float)
            local=np.linalg.solve(matrices[parent][:3,:3],world) if parent>=0 else world
            deltas.append({'path':paths[name],'translation':local.tolist(),'rotation':[0,0,0,1]})
            bone=rig.pose.bones[name];matrix=rest[name].copy();matrix.translation+=inverse.to_3x3()@Vector(delta);bone.matrix=matrix
            influences[name]=influence_counts.get(name,0)
        bpy.context.view_layer.update();current=vertices();distance=np.linalg.norm(current-base,axis=1)
        expression={'schemaVersion':1,'id':preset['id'],'skeletonFingerprint':inspection['skeletonFingerprint'],'bones':deltas,'morphs':[]}
        (args.output/(preset['name']+'.expression.json')).write_text(json.dumps(expression,indent=2),encoding='utf-8')
        np.savez_compressed(args.output/(preset['name']+'.vertices.npz'),positions=current)
        scene.render.filepath=str((args.output/(preset['name']+'.png')).resolve());bpy.ops.render.render(write_still=True)
        reports.append({'name':preset['name'],'sourceKind':'authored','changedVertices':int(np.count_nonzero(distance>1e-7)),
                        'maximumDisplacementMetres':float(distance.max()),'boneInfluencedVertices':influences})
    manifest={'schemaVersion':1,'sourceKind':'authored','sourceSha256':hashlib.sha256(args.input.read_bytes()).hexdigest(),
              'configSha256':hashlib.sha256(args.config.read_bytes()).hexdigest(),'blenderVersion':bpy.app.version_string,
              'space':'gltf-world','units':'metres','vertexCount':len(base),'presets':reports,
              'enginePreparedGeometry':prepared is not None,'inspectionSha256':hashlib.sha256(args.inspection.read_bytes()).hexdigest(),
              'scope':'Technical translation poses; artistic and native-expression validation pending'}
    (args.output/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8');print(json.dumps(manifest,indent=2))


if __name__=='__main__':main()
