"""Bake the verified Lit normal graph against its source arithmetic on CPU."""
import argparse
import copy
import json
from pathlib import Path
import sys

import bpy
import numpy as np

parser=argparse.ArgumentParser(description=__doc__)
for name in ('extractor-src','manifest','material','output'):
    parser.add_argument('--'+name,required=True)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
engine=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(engine/'tools/blender'),str(engine/'tools'),args.extractor_src]
from endfield_scene_material_contract import load_catalog
from native_surface_materials import create_material

materials,textures=load_catalog((Path(args.manifest),))
source=next(row for row in materials.values() if row['Name']==args.material)
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU'
scene.cycles.samples=1;scene.render.bake.margin=0
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.mesh.primitive_plane_add(size=2)
plane=bpy.context.object
output=Path(args.output);output.mkdir(parents=True,exist_ok=False)
rows=[]
# The source clears signed XY below 0.012, scales XY only, and floors Z at 1/2048.
cases=[('neutral',.5,.5,1),('quantized-neutral',128/255,128/255,1),
       ('inside-dead-zone',.50595,.49405,1),('outside-dead-zone',.50605,.49395,1),
       ('scaled-xy',.8,.3,.37),('z-floor',1,1,1)]
for name,r,g,strength in cases:
    record=copy.deepcopy(source);record['Floats']['_NormalScale']=strength
    material=create_material(record,textures,True)
    tree=material.node_tree;nodes,links=tree.nodes,tree.links
    texture=next(node for node in nodes if node.type=='TEX_IMAGE' and node.label=='_NormalMap')
    image=bpy.data.images.new(name+'-input',width=1,height=1,alpha=True,float_buffer=True)
    image.colorspace_settings.name='Non-Color';image.alpha_mode='CHANNEL_PACKED'
    image.pixels[:]=[r,g,.4,1];texture.image=image
    bsdf=next(node for node in nodes if node.type=='BSDF_PRINCIPLED')
    normal=bsdf.inputs['Normal'].links[0].from_socket
    scale=nodes.new('ShaderNodeVectorMath');scale.operation='SCALE';scale.inputs['Scale'].default_value=.5
    links.new(normal,scale.inputs[0])
    encode=nodes.new('ShaderNodeVectorMath');encode.operation='ADD';encode.inputs[1].default_value=(.5,.5,.5)
    links.new(scale.outputs['Vector'],encode.inputs[0])
    emission=nodes.new('ShaderNodeEmission');links.new(encode.outputs['Vector'],emission.inputs['Color'])
    root=next(node for node in nodes if node.type=='OUTPUT_MATERIAL');links.new(emission.outputs[0],root.inputs['Surface'])
    target=bpy.data.images.new(name+'-bake',width=16,height=16,alpha=True,float_buffer=True)
    target.colorspace_settings.name='Non-Color';node=nodes.new('ShaderNodeTexImage');node.image=target;nodes.active=node
    plane.data.materials.clear();plane.data.materials.append(material)
    bpy.ops.object.bake(type='EMIT',margin=0)
    actual=np.asarray(target.pixels[:],dtype=float).reshape(16,16,4)[8,8,:3]*2-1
    xy=np.asarray([r,g],dtype=np.float32)*np.float32(2)-np.float32(1)
    xy=np.where(np.abs(xy)<np.float32(.012),np.float32(0),xy)
    z=max(float(np.sqrt(max(0,1-float(np.dot(xy,xy))))),1/2048)
    expected=np.asarray([xy[0]*strength,xy[1]*strength,z]);expected/=np.linalg.norm(expected)
    error=float(np.max(np.abs(actual-expected)))
    rows.append(dict(name=name,input=[r,g],strength=strength,actual=actual.tolist(),expected=expected.tolist(),maximumError=error))
for invalid in (-1,float('inf'),float('nan'),True):
    record=copy.deepcopy(source);record['Floats']['_NormalScale']=invalid
    count=len(bpy.data.materials)
    try:create_material(record,textures,True)
    except ValueError as error:assert 'normal strength' in str(error)
    else:raise AssertionError('Invalid normal strength accepted')
    assert len(bpy.data.materials)==count,'Invalid input created a material'
result=dict(status='passed' if max(row['maximumError'] for row in rows)<2e-5 else 'failed',
            device=scene.cycles.device,budget=2e-5,cases=rows,invalidStrengthCases=4)
(output/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
assert result['status']=='passed',json.dumps(result)
print('Verified native Lit normal graph:',len(rows),'cases')
