"""Generate original CC0 humanoids with authored idle/walk limb motion."""
import base64
import json
import math
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[1]/'assets_public/third_person'

def character(name,robot=False):
    binary=bytearray();views=[];accessors=[]
    def accessor(rows,kind,component=5126):
        binary.extend(b'\0'*(-len(binary)%4));offset=len(binary)
        values=[v for row in rows for v in (row if isinstance(row,(list,tuple)) else [row])]
        binary.extend(struct.pack('<'+{5126:'f',5123:'H'}[component]*len(values),*values))
        view=len(views);views.append({'buffer':0,'byteOffset':offset,'byteLength':len(binary)-offset})
        record={'bufferView':view,'componentType':component,'count':len(rows),'type':kind}
        if kind=='SCALAR':record.update(min=[min(values)],max=[max(values)])
        if kind=='VEC3':record.update(min=[min(row[i] for row in rows) for i in range(3)],max=[max(row[i] for row in rows) for i in range(3)])
        index=len(accessors);accessors.append(record);return index
    joints=[('hips',-1,[0,.9,0]),('spine',0,[0,.3,0]),('head',1,[0,.43,0]),
        ('left-arm',1,[.27,.21,0]),('left-forearm',3,[0,-.27,0]),('left-hand',4,[0,-.25,0]),
        ('right-arm',1,[-.27,.21,0]),('right-forearm',6,[0,-.27,0]),('right-hand',7,[0,-.25,0]),
        ('left-thigh',0,[.13,-.02,0]),('left-calf',9,[0,-.4,0]),('left-foot',10,[0,-.4,0]),
        ('right-thigh',0,[-.13,-.02,0]),('right-calf',12,[0,-.4,0]),('right-foot',13,[0,-.4,0])]
    world=[];nodes=[{'name':name,'mesh':0,'skin':0}]
    for joint,parent,t in joints:
        world.append([t[a]+(world[parent][a] if parent>=0 else 0) for a in range(3)])
        nodes.append({'name':joint,'translation':t})
        if parent>=0:nodes[parent+1].setdefault('children',[]).append(len(nodes)-1)
    positions=[];normals=[];uvs=[];bones=[];weights=[];indices=[];morph=[]
    def box(joint,center,extent):
        for axis in range(3):
            a,b=(axis+1)%3,(axis+2)%3
            for sign in (-1,1):
                start=len(positions)
                corners=[(-1,-1),(1,-1),(1,1),(-1,1)]
                if sign<0:corners.reverse()
                for ca,cb in corners:
                    p=[world[joint][i]+center[i] for i in range(3)];p[axis]+=sign*extent[axis];p[a]+=ca*extent[a];p[b]+=cb*extent[b]
                    positions.append(p);n=[0,0,0];n[axis]=sign;normals.append(n);uvs.append([(ca+1)/2,(cb+1)/2])
                    bones.append([joint,0,0,0]);weights.append([1,0,0,0]);morph.append([p[0]*.06,0,0] if joint==2 else [0,0,0])
                indices.extend([start,start+1,start+2,start,start+2,start+3])
    box(0,[0,0,0],[.21,.13,.12]);box(1,[0,.06,0],[.23,.24,.12])
    box(2,[0,0,0],[.18 if robot else .15,.17,.14])
    for joint in (3,6):box(joint,[0,-.13,0],[.075,.14,.075])
    for joint in (4,7):box(joint,[0,-.12,0],[.065,.125,.065])
    for joint in (5,8):box(joint,[0,-.045,0],[.07,.07,.075])
    for joint in (9,12):box(joint,[0,-.2,0],[.09,.21,.09])
    for joint in (10,13):box(joint,[0,-.2,0],[.08,.21,.08])
    for joint in (11,14):box(joint,[0,-.02,-.08],[.095,.07,.18])
    if robot:box(2,[0,.24,0],[.025,.08,.025])
    primitive={'attributes':{'POSITION':accessor(positions,'VEC3'),'NORMAL':accessor(normals,'VEC3'),
        'TEXCOORD_0':accessor(uvs,'VEC2'),'JOINTS_0':accessor(bones,'VEC4',5123),'WEIGHTS_0':accessor(weights,'VEC4')},
        'indices':accessor(indices,'SCALAR',5123),'material':0,'targets':[{'POSITION':accessor(morph,'VEC3')}]}
    inverse=[]
    for p in world:inverse.append([1,0,0,0,0,1,0,0,0,0,1,0,-p[0],-p[1],-p[2],1])
    skin={'joints':list(range(1,len(nodes))),'inverseBindMatrices':accessor(inverse,'MAT4'),'skeleton':1}
    animations=[];times=[i/30 for i in range(31)]
    for clip in ('idle','walk'):
        animation={'name':clip,'channels':[],'samplers':[]};time_accessor=accessor(times,'SCALAR')
        def track(joint,path,rows,kind):
            index=len(animation['samplers']);animation['samplers'].append({'input':time_accessor,'output':accessor(rows,kind),'interpolation':'LINEAR'})
            animation['channels'].append({'sampler':index,'target':{'node':joint+1,'path':path}})
        for joint in (3,4,6,7,9,10,12,13):
            angles=[]
            for time in times:
                phase=math.sin(time*2*math.pi)
                if clip=='idle':angle=.018*phase*(1 if joint in (3,6) else .2)
                elif joint in (9,12):angle=.58*phase*(1 if joint==9 else -1)
                elif joint in (10,13):angle=-max(0,phase*(1 if joint==10 else -1))*.7
                elif joint in (3,6):angle=.4*phase*(-1 if joint==3 else 1)
                else:angle=-.15
                angles.append([math.sin(angle/2),0,0,math.cos(angle/2)])
            track(joint,'rotation',angles,'VEC4')
        track(0,'translation',[[0,.9+(.008 if clip=='idle' else .018)*math.sin(time*math.pi*4),0] for time in times],'VEC3')
        animations.append(animation)
    doc={'asset':{'version':'2.0','generator':'AzureRender original CC0 character generator',
        'extras':{'license':'CC0-1.0','motionReferenceMetersPerSecond':2}},
        'scene':0,'scenes':[{'nodes':[0,1]}],'nodes':nodes,'skins':[skin],
        'meshes':[{'name':name,'primitives':[primitive],'weights':[0]}],
        'materials':[{'name':name+'-cloth','pbrMetallicRoughness':{'baseColorFactor':[.2,.62,.8,1] if robot else [.9,.36,.16,1],'metallicFactor':.1,'roughnessFactor':.7}}],
        'animations':animations,'bufferViews':views,'accessors':accessors,
        'buffers':[{'byteLength':len(binary),'uri':'data:application/octet-stream;base64,'+base64.b64encode(binary).decode()}]}
    ROOT.mkdir(parents=True,exist_ok=True);(ROOT/(name+'.gltf')).write_text(json.dumps(doc,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':
    character('explorer');character('guide',True)
