"""Export native vertex identifiers beside the reference character geometry."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
from mathutils import Matrix
import numpy as np


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('blend','geometry','reference','output','manifest'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    if args.output.exists() or args.manifest.exists() or args.output.resolve()==args.manifest.resolve():
        raise ValueError('Outputs must be distinct new files')
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
    from convert_material_semantics import read_asset
    digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    native=json.loads(args.geometry.read_text(encoding='utf8'))
    binary_path=(args.geometry.parent/native['binary_path']).resolve()
    if not binary_path.is_relative_to(args.geometry.parent.resolve()):raise ValueError('Source binary escapes geometry root')
    binary=binary_path.read_bytes()
    if len(binary)!=native['binary_bytes'] or hashlib.sha256(binary).hexdigest()!=native['binary_sha256']:
        raise ValueError('Native geometry fingerprint differs')
    document,_=read_asset(args.reference);required={mesh['name'] for mesh in document['meshes']}
    meshes={mesh['path']:mesh for mesh in native['meshes']}
    bpy.ops.wm.open_mainfile(filepath=str(args.blend.resolve(strict=True)))
    bpy.context.scene.frame_set(1);bpy.ops.object.select_all(action='DESELECT')
    conversion=Matrix(((1,0,0,0),(0,0,1,0),(0,-1,0,0),(0,0,0,1)))
    rows=[]
    for obj in bpy.context.scene.objects:
        if obj.type=='ARMATURE':obj.hide_set(False);obj.select_set(True)
        if obj.type!='MESH' or obj.data.name_full not in required:continue
        row=meshes[obj['source_geometry_path']];count=row['vertex_count']
        if len(obj.data.vertices)!=count:raise ValueError('Source mesh vertex count changed')
        positions=np.empty(count*3,dtype=np.float32);obj.data.vertices.foreach_get('co',positions)
        expected=np.frombuffer(binary,'<f4',offset=row['positions']['offset'],count=count*3)
        if not np.array_equal(positions,expected):raise ValueError('Source positions changed')
        expected=np.frombuffer(binary,'<f4',offset=row['tangents']['offset'],count=count*4).reshape(-1,4)
        tangent=np.empty(count*3,dtype=np.float32);obj.data.attributes['source_tangent'].data.foreach_get('vector',tangent)
        signs=np.empty(count,dtype=np.float32);obj.data.attributes['source_tangent_sign'].data.foreach_get('value',signs)
        if not np.array_equal(tangent.reshape(-1,3),expected[:,:3]) or not np.array_equal(signs,expected[:,3]):
            raise ValueError('Source tangents changed')
        attribute=obj.data.attributes.new('_AZURE_SOURCE_VERTEX',type='INT',domain='POINT')
        attribute.data.foreach_set('value',np.arange(count,dtype=np.int32))
        obj.hide_set(False);obj.select_set(True)
        rows.append({'name':obj.data.name_full,'source_path':row['path'],
                     'vertex_transform':[list(line) for line in conversion@obj.matrix_world]})
    if {row['name'] for row in rows}!=required or len(rows)!=len(required):
        raise ValueError('Reference mesh set must resolve exactly once')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(args.output.resolve()),export_format='GLB',use_selection=True,
        export_animations=False,export_morph=False,export_materials='NONE',export_attributes=True,
        export_extras=True,export_tangents=False,export_yup=True)
    report={'geometry_sha256':digest(args.geometry),'binary_sha256':native['binary_sha256'],
            'blend_sha256':digest(args.blend),'reference_sha256':digest(args.reference),
            'generator_sha256':digest(Path(__file__)),'output_sha256':digest(args.output),
            'blender_version':bpy.app.version_string,'source_positions_and_tangents_exact':True,'meshes':rows}
    args.manifest.parent.mkdir(parents=True,exist_ok=True)
    args.manifest.write_text(json.dumps(report,indent=2),encoding='utf8')
    print('PASS native vertex correspondence',len(rows),'meshes',flush=True)


if __name__=='__main__':main()
