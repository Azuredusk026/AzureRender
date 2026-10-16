"""Export Blender scene hierarchy and shared meshes as an Azure scene exchange package."""
import argparse
import hashlib
import json
import math
import os
import sys
from pathlib import Path


def write_exchange(document, output):
    output = Path(output)
    if document.get('schemaVersion') != 1:
        raise ValueError('Unsupported scene exchange version')
    if document.get('upAxis') != 'Y' or not math.isfinite(document.get('unitMeters', 0)) or document['unitMeters'] <= 0:
        raise ValueError('Invalid coordinate contract')
    resources = {}
    for resource in document['resources']:
        path = (output.parent / resource['path']).resolve()
        if not resource['id'] or resource['id'] in resources or not path.is_relative_to(output.parent.resolve()) or not path.is_file():
            raise ValueError('Invalid or missing resource')
        resources[resource['id']] = path
    parents = {}
    for node in document['nodes']:
        if not node['id'] or node['id'] in parents:
            raise ValueError('Duplicate node ID')
        parents[node['id']] = node.get('parent', '')
        if node.get('meshRef') and node['meshRef'] not in resources:
            raise ValueError('Missing mesh resource')
        for value in node.get('localTransform', {}).values():
            if not isinstance(value, list) or not all(math.isfinite(v) for v in value):
                raise ValueError('Nonfinite transform')
    for node in parents:
        visited = set()
        while node:
            if node not in parents:
                raise ValueError('Missing parent')
            if node in visited:
                raise ValueError('Hierarchy cycle')
            visited.add(node)
            node = parents[node]
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + '.tmp')
    try:
        temporary.write_text(json.dumps(document, ensure_ascii=False, indent=2), encoding='utf-8')
        os.replace(temporary, output)
    finally:
        temporary.unlink(missing_ok=True)


def export_scene(output, scene_id):
    import bpy
    from mathutils import Matrix
    output = Path(output)
    if output.exists():
        raise FileExistsError('Export into a fresh candidate package')
    output.parent.mkdir(parents=True, exist_ok=True)
    scene = bpy.context.scene
    objects = sorted(scene.objects, key=lambda obj: obj.name_full)
    identities = {obj: obj.get('azureSourceId', obj.name_full) for obj in objects}
    units = scene.unit_settings.scale_length
    conversion = Matrix(((1,0,0,0),(0,0,1,0),(0,-1,0,0),(0,0,0,1)))
    resources, nodes, lights, losses = [], [], [], []
    mesh_ids = {}
    selected = list(bpy.context.selected_objects)
    active = bpy.context.view_layer.objects.active
    try:
        for obj in objects:
            node = dict(id=identities[obj], name=obj.name, parent=identities.get(obj.parent, ''), visible=not obj.hide_render)
            matrix = conversion @ obj.matrix_local @ conversion.inverted()
            for axis in range(3): matrix[axis][3] *= units
            node['localTransform'] = dict(matrix=[matrix[row][column] for column in range(4) for row in range(4)])
            if obj.type == 'MESH':
                key = (obj.data.name_full, tuple(slot.material.name_full if slot.material else '' for slot in obj.material_slots))
                if key not in mesh_ids:
                    identity = 'mesh-' + hashlib.sha256(repr(key).encode()).hexdigest()[:16]
                    mesh_ids[key] = identity
                    candidate = bpy.data.objects.new(identity, obj.data.copy())
                    for vertex in candidate.data.vertices: vertex.co *= units
                    scene.collection.objects.link(candidate)
                    for index, slot in enumerate(obj.material_slots):
                        if index < len(candidate.material_slots): candidate.material_slots[index].material = slot.material
                    bpy.ops.object.select_all(action='DESELECT')
                    candidate.select_set(True)
                    bpy.context.view_layer.objects.active = candidate
                    path = output.parent / (identity + '.glb')
                    try:
                        bpy.ops.export_scene.gltf(filepath=str(path.resolve()), export_format='GLB', use_selection=True, export_animations=False, export_yup=True)
                    finally:
                        mesh = candidate.data
                        bpy.data.objects.remove(candidate, do_unlink=True)
                        bpy.data.meshes.remove(mesh)
                    resources.append(dict(id=identity, path=path.name, sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
                node['meshRef'] = mesh_ids[key]
                if obj.modifiers:
                    losses.append(dict(code='MeshModifiers', source=node['id'], message='Mesh exports base data; modifier evaluation requires a prepared asset'))
            elif obj.type == 'LIGHT':
                light = obj.data
                lights.append(dict(id=node['id']+'-light', node=node['id'], type='point' if light.type == 'POINT' else light.type.lower(), color=list(light.color), intensity=light.energy, radius=max(light.cutoff_distance*units, .001)))
                losses.append(dict(code='LightIntensity', source=node['id'], message='Blender energy is recorded; renderer intensity units need appearance validation'))
            elif obj.type == 'CAMERA':
                camera = obj.data
                node['camera'] = dict(enabled=True, priority=0, orthographic=camera.type == 'ORTHO', fieldOfView=math.degrees(camera.angle_y), orthographicHeight=camera.ortho_scale*units, nearPlane=camera.clip_start*units, farPlane=camera.clip_end*units)
            elif obj.type not in {'EMPTY'}:
                losses.append(dict(code='UnsupportedObject', source=node['id'], message='Only hierarchy retained for '+obj.type))
            nodes.append(node)
        document = dict(schemaVersion=1, id=scene_id, unitMeters=1, upAxis='Y', resources=resources, nodes=nodes, lights=lights, losses=losses,
                        generator=dict(name='AzureRender scene exchange', version=1, blenderVersion=bpy.app.version_string))
        write_exchange(document, output)
        return document
    finally:
        bpy.ops.object.select_all(action='DESELECT')
        for obj in selected:
            if obj.name in bpy.data.objects: obj.select_set(True)
        bpy.context.view_layer.objects.active = active


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--scene-id', required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    export_scene(args.output, args.scene_id)

if __name__ == '__main__': main()
