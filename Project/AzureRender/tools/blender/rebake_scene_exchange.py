"""Rebuild exchange mesh materials from an unbaked Blender source scene."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy
sys.path.insert(0,str(Path(__file__).resolve().parent))
from bake_material_channels import bake_material


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--reference',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--resolution',type=int,default=1024)
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    source=args.source.resolve(strict=True);reference=args.reference.resolve(strict=True)
    output=args.output.resolve()
    output.mkdir(parents=True,exist_ok=False)
    document=json.loads(reference.read_text(encoding='utf8'))
    if document.get('schemaVersion')!=1 or document.get('upAxis')!='Y':
        raise ValueError('Unsupported reference exchange contract')
    bpy.ops.wm.open_mainfile(filepath=str(source))
    scene=bpy.context.scene;scene.cycles.device='CPU'
    objects=list(scene.objects)
    representatives={}
    for obj in objects:
        if obj.type!='MESH':continue
        key=(obj.data.name_full,tuple(slot.material.name_full if slot.material else '' for slot in obj.material_slots))
        identity='mesh-'+hashlib.sha256(repr(key).encode()).hexdigest()[:16]
        representatives.setdefault(identity,obj)
    required={record['id'] for record in document['resources']}
    missing=required-set(representatives)
    if missing:raise ValueError('Source lacks reference mesh identities: '+repr(sorted(missing)))
    materials=sorted({slot.material for key in required for slot in representatives[key].material_slots if slot.material},key=lambda m:m.name)
    if any('azureBakeDomain' in mat for mat in materials):
        raise ValueError('Rebaking requires original unbaked materials')
    record={'source':str(source),'source_sha256':digest(source),'reference':str(reference),
        'reference_sha256':digest(reference),'generator_sha256':digest(Path(__file__)),
        'baker_sha256':digest(Path(__file__).with_name('bake_material_channels.py')),
        'blender_version':bpy.app.version_string,'resolution':args.resolution,
        'resources':len(required),'materials':len(materials),'bakes':[],
        'node_contract':'Reference node IDs, hierarchy, transforms and mesh references preserved'}
    replacements={}
    for index,material in enumerate(materials):
        name=material.name
        baked,report=bake_material(material,args.resolution,material.get('endfield_alpha_clip_threshold'))
        material.name=name+'-unbaked-source';baked.name=name
        replacements[material]=baked;record['bakes'].append(report)
        print('BAKE',index+1,len(materials),name,flush=True)
    for index,resource in enumerate(document['resources']):
        obj=representatives[resource['id']]
        path=(output/resource['path']).resolve()
        if not path.is_relative_to(output):raise ValueError('Exchange path escapes output')
        path.parent.mkdir(parents=True,exist_ok=True)
        candidate=bpy.data.objects.new(resource['id'],obj.data.copy())
        for vertex in candidate.data.vertices:vertex.co*=scene.unit_settings.scale_length
        scene.collection.objects.link(candidate)
        for slot_index,slot in enumerate(obj.material_slots):
            if slot.material:candidate.material_slots[slot_index].material=replacements[slot.material]
        bpy.ops.object.select_all(action='DESELECT');candidate.select_set(True)
        bpy.context.view_layer.objects.active=candidate
        try:
            bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,
                export_animations=False,export_yup=True)
        finally:
            mesh=candidate.data;bpy.data.objects.remove(candidate,do_unlink=True);bpy.data.meshes.remove(mesh)
        resource['sha256']=digest(path)
        print('EXPORT',index+1,len(required),resource['id'],flush=True)
    document['generator']={'name':'AzureRender source material rebake','version':1,'blenderVersion':bpy.app.version_string}
    exchange=output/reference.name
    exchange.write_text(json.dumps(document,ensure_ascii=False,indent=2),encoding='utf8')
    before=json.loads(reference.read_text(encoding='utf8'))
    assert document['nodes']==before['nodes']
    assert [(r['id'],r['path']) for r in document['resources']]==[(r['id'],r['path']) for r in before['resources']]
    record['exchange_sha256']=digest(exchange)
    record['resource_ids_equal']=True;record['nodes_equal']=True
    record['total_glb_bytes']=sum((output/item['path']).stat().st_size for item in document['resources'])
    (output/'rebake-manifest.json').write_text(json.dumps(record,indent=2),encoding='utf8')
    print('PASS',len(materials),'materials',len(required),'resources; reference node/ID contract preserved',flush=True)


if __name__=='__main__':main()
