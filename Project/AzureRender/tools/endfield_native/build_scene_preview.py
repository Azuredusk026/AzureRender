"""Build a source-bound static scene candidate inside factory-startup Blender.

The pinned upstream supplies OBJ coordinates and approximate material nodes.
Every selected draw keeps its source entity, mesh, material and submesh identity.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

import bpy
from mathutils import Matrix, Vector


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'probe', 'inventory', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--exchange', action='store_true')
    parser.add_argument('--render', action='store_true')
    parser.add_argument('--bake-materials', type=int, metavar='RESOLUTION')
    parser.add_argument('--mesh-bake-uv', choices=('UV0','UV1'),
                        help='Bake the verified surface on an explicit native mesh UV layer')
    parser.add_argument('--surface-color-encoding', choices=('authored-srgb','gpu-linear'), default='authored-srgb')
    parser.add_argument('--lod-policy', choices=('metric', 'highest'), default='metric')
    parser.add_argument('--lod-metric-squared', type=float)
    parser.add_argument('--source-chunk', action='append',
                        help='Exact inventory chunk path; repeat to select multiple sources')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.lod_policy == 'highest' and args.lod_metric_squared is not None:
        parser.error('--lod-metric-squared requires --lod-policy metric')
    if args.mesh_bake_uv and not args.bake_materials:
        parser.error('--mesh-bake-uv requires --bake-materials')
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from endfield_native.scene_lod import select_draw_indices
    from endfield_native.mesh_binding import resolve_mesh_binding
    from endfield_native.scene_sources import select_renderer_sources
    source = args.source_root.resolve(strict=True)
    probe = args.probe.resolve(strict=True)
    inventory = args.inventory.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    sys.path.insert(0, str(source))
    from blender_build_map02_chunk_20260727 import unity_matrix
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'blender'))
    from native_mesh_frames import import_native_obj as import_obj
    from native_surface_materials import create_material,configure_uv_sampling
    from endfield_scene_material_contract import load_catalog, reference_key
    manifest_path = probe / 'scene_manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf8'))
    data = json.loads(inventory.read_text(encoding='utf8'))
    if data['format'] != 'AzureEndfieldRawSceneInventory/1':
        raise ValueError('Unsupported source instance inventory')
    selected_renderers, source_selection = select_renderer_sources(data, args.source_chunk)
    materials, textures = load_catalog((manifest_path,))
    containers = {}
    for row in manifest['Containers']:
        path = row['Container'].casefold()
        if path in containers and containers[path] != row['Asset']:
            raise ValueError('Ambiguous source container: ' + path)
        containers[path] = row['Asset']
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    mesh_cache, material_cache, draw_cache = {}, {}, {}
    bake_reports = []
    uv_reports = []
    normal_reports = []
    audit = []
    max_matrix_error = 0.0
    basis = Matrix(((1,0,0,0), (0,0,-1,0), (0,1,0,0), (0,0,0,1)))
    for row in selected_renderers:
        entity = row['instance']
        identity = 'native-' + str(entity['entity_id'])
        parent = bpy.data.objects.new(identity, None)
        bpy.context.collection.objects.link(parent)
        parent['azureSourceId'] = identity
        parent['nativeSourceChunk'] = entity['source']
        parent.matrix_world = unity_matrix(struct.pack('<16f', *entity['matrix']))
        reconstructed = basis.inverted() @ parent.matrix_world @ basis
        max_matrix_error = max(max_matrix_error, max(abs(reconstructed[r][c] - entity['matrix'][c*4+r]) for r in range(4) for c in range(4)))
        selected = select_draw_indices(row['pairs'], policy=args.lod_policy,
            metric_squared=args.lod_metric_squared if args.lod_metric_squared is not None else 0.0)
        for index in selected:
            pair = row['pairs'][index]
            draw = pair['draw']
            asset, exported, alias = resolve_mesh_binding(pair['mesh'], containers, manifest['MeshExports'])
            key = reference_key(asset)
            if key not in mesh_cache:
                objects = import_obj(probe / exported['File'])
                mesh_cache[key] = [obj.data for obj in objects]
                normal_reports.extend(json.loads(mesh['azureNativeNormalReceipt']) for mesh in mesh_cache[key])
                if args.mesh_bake_uv:
                    from native_mesh_uvs import restore_uv1
                    uv_reports.append(restore_uv1(mesh_cache[key],exported,probe))
                for obj in objects:
                    bpy.data.objects.remove(obj, do_unlink=True)
            material_key = reference_key(containers[pair['material']])
            if material_key not in material_cache:
                material_cache[material_key] = create_material(materials[material_key], textures, True,
                    color_encoding=args.surface_color_encoding)
                if args.mesh_bake_uv:
                    configure_uv_sampling(material_cache[material_key],materials[material_key])
                if args.bake_materials and not args.mesh_bake_uv:
                    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'blender'))
                    from bake_material_channels import bake_material
                    original = material_cache[material_key]
                    baked, bake_report = bake_material(original, args.bake_materials,
                        original.get('endfield_alpha_clip_threshold'))
                    name = original.name
                    original.name = name + '-source'
                    baked.name = name
                    material_cache[material_key] = baked
                    bake_reports.append(bake_report)
            draw_key = (key, draw['submesh'], material_key)
            if draw_key not in draw_cache:
                mesh = mesh_cache[key][draw['submesh']].copy()
                mesh.materials.clear()
                mesh.materials.append(material_cache[material_key])
                if args.mesh_bake_uv:
                    from bake_material_channels import bake_material
                    original=material_cache[material_key]
                    baked,bake_report=bake_material(original,args.bake_materials,
                        original.get('endfield_alpha_clip_threshold'),mesh=mesh,uv_layer=args.mesh_bake_uv)
                    mesh.materials.clear();mesh.materials.append(baked)
                    mesh.uv_layers.active=mesh.uv_layers[args.mesh_bake_uv]
                    mesh.uv_layers[args.mesh_bake_uv].active_render=True
                    bake_reports.append(bake_report)
                draw_cache[draw_key] = mesh
            mesh = draw_cache[draw_key]
            obj = bpy.data.objects.new(identity + '-draw-' + str(index), mesh)
            bpy.context.collection.objects.link(obj)
            obj.parent = parent
            obj.matrix_basis = Matrix.Identity(4)
            obj['azureSourceId'] = obj.name
            obj['source_entity_id'] = entity['entity_id']
            obj['source_mesh'] = pair['mesh']
            obj['source_material'] = pair['material']
            obj['source_submesh'] = draw['submesh']
            audit.append(dict(entity=entity['entity_id'], mesh=pair['mesh'], material=pair['material'],
                              submesh=draw['submesh'], vertices=len(mesh.vertices),
                              source_draw_index=index,
                              source_mesh_object=asset,
                              source_mesh_label_alias=alias,
                              lod_interval=[draw['lod_min_squared'], draw['lod_max_squared']],
                              triangles=sum(len(p.vertices)-2 for p in mesh.polygons)))
    if max_matrix_error > 1e-4:
        raise ValueError('Source world matrix round-trip exceeds tolerance')
    bpy.context.view_layer.update()
    observed_bounds = {}
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH':
            continue
        entity_id = obj['source_entity_id']
        bounds = observed_bounds.setdefault(entity_id, [float('inf'), -float('inf')] * 3)
        transform = basis.inverted() @ obj.matrix_world
        used_vertices = {index for polygon in obj.data.polygons for index in polygon.vertices}
        for index in used_vertices:
            point = transform @ obj.data.vertices[index].co
            for axis in range(3):
                bounds[axis*2] = min(bounds[axis*2], point[axis])
                bounds[axis*2+1] = max(bounds[axis*2+1], point[axis])
    maximum_bounds_overflow = 0.0
    for row in selected_renderers:
        entity = row['instance']
        actual = observed_bounds.get(entity['entity_id'])
        expected = entity['world_bounds']
        if actual is None or expected is None:
            raise ValueError('Selected entity lacks geometry or source bounds')
        maximum_bounds_overflow = max(maximum_bounds_overflow, max(
            max(expected[a*2]-actual[a*2], actual[a*2+1]-expected[a*2+1], 0)
            for a in range(3)))
    if maximum_bounds_overflow > 1e-4:
        raise ValueError('Selected geometry exceeds source world bounds')
    if args.exchange:
        sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'blender'))
        from export_scene_exchange import export_scene
        exchange = export_scene(output / 'exchange/scene.azuresceneexchange', data['scene'] + '-native-slice',
                                export_tangents=bool(args.mesh_bake_uv))
    else:
        exchange = None
    bpy.ops.export_scene.gltf(filepath=str(output / 'scene.glb'), export_format='GLB', export_animations=False,
                              export_extras=True,export_tangents=bool(args.mesh_bake_uv))
    bpy.ops.wm.save_as_mainfile(filepath=str(output / 'scene.blend'))
    uv_dependencies = ([Path(__file__).resolve().parents[1]/'endfield_native/mesh_uvs.py',
        Path(__file__).resolve().parents[1]/'blender/native_mesh_uvs.py',
        Path(__file__).resolve().parents[1]/'blender/bake_material_channels.py',
        Path(__file__).resolve().parents[1]/'blender/export_scene_exchange.py'] if args.mesh_bake_uv else [])
    report = dict(status='source_bound_technical_preview', objects=audit,
                  source_entities=len(selected_renderers), shared_draw_resources=len(draw_cache),
                  source_selection=source_selection,
                  maximum_source_matrix_error=max_matrix_error, materials=len(material_cache),
                  maximum_source_bounds_overflow=maximum_bounds_overflow,
                  inputs=[dict(path=str(p), sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in (inventory, manifest_path, Path(__file__),
                      Path(__file__).resolve().parents[1] / 'blender/native_surface_materials.py',
                      Path(__file__).resolve().parents[1] / 'blender/native_mesh_frames.py',
                      Path(__file__).with_name('mesh_corners.py'),
                      Path(__file__).with_name('scene_sources.py'),
                      source / 'blender_scene_materials.py',source / 'blender_build_map02_chunk_20260727.py',*uv_dependencies)],
                  exchange_resources=len(exchange['resources']) if exchange else None,
                  baked_materials=bake_reports,
                  mesh_bake_uv=args.mesh_bake_uv,uv_streams=uv_reports,normal_streams=normal_reports,
                  lod_policy=args.lod_policy,
                  lod_metric_squared=(args.lod_metric_squared or 0.0) if args.lod_policy == 'metric' else None,
                  limitations=['static authored LOD selection; runtime camera mapping unverified', 'upstream material approximation',
                               'whole meshes retained at slice boundary', 'game lighting and effects absent'])
    (output / 'audit.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    if args.render:
        scene = bpy.context.scene
        scene.render.engine = 'CYCLES'
        scene.cycles.samples = 12
        scene.render.resolution_x, scene.render.resolution_y = 960, 640
        scene.render.resolution_percentage = 100
        scene.world.color = (.25, .25, .25)
        bpy.ops.object.camera_add(location=(28,32,25))
        camera = bpy.context.object
        camera.rotation_euler = (Vector((0,0,2))-camera.location).to_track_quat('-Z','Y').to_euler()
        camera.data.type, camera.data.ortho_scale = 'ORTHO', 55
        scene.camera = camera
        bpy.ops.object.light_add(type='SUN', location=(10,10,30))
        bpy.context.object.rotation_euler = (.4,-.5,.3)
        bpy.context.object.data.energy = 2
        scene.render.filepath = str(output / 'scene.png')
        bpy.ops.render.render(write_still=True)


if __name__ == '__main__':
    main()
