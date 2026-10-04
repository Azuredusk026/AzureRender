"""Retarget sampled FBX world rotations onto a GLB skeleton, preserving source chunks."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import numpy as np


def qmatrix(q):
    q = np.array(q, dtype=float)
    if not np.isfinite(q).all() or np.linalg.norm(q) < 1e-8:
        raise ValueError("Invalid quaternion")
    x, y, z, w = q / np.linalg.norm(q)
    return np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                     [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                     [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])


def quaternion(m):
    # Largest diagonal branch is stable near a half turn.
    values = [1+m[0,0]-m[1,1]-m[2,2], 1-m[0,0]+m[1,1]-m[2,2],
              1-m[0,0]-m[1,1]+m[2,2], 1+np.trace(m)]
    i = int(np.argmax(values)); v = np.sqrt(max(values[i], 0)) * .5
    q = np.zeros(4); q[i] = v; denominator = 4*v
    if i == 3:
        q[:3] = [(m[2,1]-m[1,2])/denominator, (m[0,2]-m[2,0])/denominator, (m[1,0]-m[0,1])/denominator]
    else:
        j, k = (i+1)%3, (i+2)%3
        q[j] = (m[j,i]+m[i,j])/denominator; q[k] = (m[k,i]+m[i,k])/denominator
        q[3] = (m[k,j]-m[j,k])/denominator
    return q / np.linalg.norm(q)


def trs_matrix(trs):
    t, q, s = trs
    m = np.eye(4); m[:3,:3] = qmatrix(q) @ np.diag(s); m[:3,3] = t
    if not np.isfinite(m).all(): raise ValueError("Invalid transform")
    return m


def world_matrices(transforms, parents):
    result = [None]*len(transforms); visiting = set()
    def visit(i):
        if result[i] is not None: return result[i]
        if i in visiting: raise ValueError("Cyclic skeleton")
        visiting.add(i); local = trs_matrix(transforms[i]); parent = parents[i]
        result[i] = visit(parent) @ local if parent >= 0 else local
        visiting.remove(i); return result[i]
    for i in range(len(result)): visit(i)
    return result


def rotation(m):
    basis = m[:3,:3] / np.linalg.norm(m[:3,:3], axis=0)
    if not np.allclose(basis.T @ basis, np.eye(3), atol=1e-4) or np.linalg.det(basis) < 0:
        raise ValueError("Skeleton needs positive scale and orthogonal bind axes")
    return basis


def bip001_mapping():
    result = {"Bip001_Pelvis": "Hips", "Bip001_Spine": "Spine", "Bip001_Spine1": "Spine1",
              "Bip001_Spine2": "Spine2", "Bip001_Neck": "Neck", "Bip001_Head": "Head"}
    for side, label in [("L", "Left"), ("R", "Right")]:
        for target, source in [("Clavicle","Shoulder"), ("UpperArm","Arm"), ("Forearm","ForeArm"),
                               ("Hand","Hand"), ("Thigh","UpLeg"), ("Calf","Leg"), ("Foot","Foot"), ("Toe0","ToeBase")]:
            result[f"Bip001_{side}_{target}"] = label+source
        for digit, name in enumerate(["Thumb", "Index", "Middle", "Ring", "Pinky"]):
            for segment in range(3):
                suffix = str(digit)+(str(segment) if segment else "")
                result[f"Bip001_{side}_Finger{suffix}"] = f"{label}Hand{name}{segment+1}"
    return result


def retarget_samples(document, source, mapping, hips, loop=True):
    nodes = document["nodes"]; names = {n.get("name", ""): i for i,n in enumerate(nodes)}
    source_names = {n["name"].split(":")[-1]: i for i,n in enumerate(source["nodes"])}
    if hips not in names: raise ValueError("Target hips node missing")
    mapped = {}
    for target, original in mapping.items():
        if target not in names or original not in source_names: raise ValueError(f"Missing mapped joint: {target} -> {original}")
        mapped[names[target]] = source_names[original]
    if names[hips] not in mapped: raise ValueError("Hips must be mapped")
    parents = [-1]*len(nodes)
    for i,n in enumerate(nodes):
        for child in n.get("children", []): parents[child] = i
    if any("matrix" in n for n in nodes): raise ValueError("Decompose target matrix nodes before retargeting")
    rest = [[n.get("translation", [0,0,0]), n.get("rotation", [0,0,0,1]), n.get("scale", [1,1,1])] for n in nodes]
    source_rest = [n["rest"] for n in source["nodes"]]; source_parents = [n["parent"] for n in source["nodes"]]
    target_world = world_matrices(rest, parents); bind_source = world_matrices(source_rest, source_parents)
    frames = source["clips"][0]["frames"]; times = np.array([f["time"] for f in frames])
    if len(times)<2 or not np.isfinite(times).all() or np.any(np.diff(times)<=0): raise ValueError("Animation keys must strictly increase")
    hip_index = names[hips]; source_hip = mapped[hip_index]
    # Use mapped leg lengths where available; translation remains in target units.
    target_leg = source_leg = 0
    for a,b in [("Bip001_L_Thigh", "Bip001_L_Calf"), ("Bip001_L_Calf", "Bip001_L_Foot")]:
        if a in names and b in names:
            ta,tb = names[a],names[b]; sa,sb = mapped[ta],mapped[tb]
            target_leg += np.linalg.norm(target_world[ta][:3,3]-target_world[tb][:3,3])
            source_leg += np.linalg.norm(bind_source[sa][:3,3]-bind_source[sb][:3,3])
    ratio = target_leg/source_leg if source_leg>1e-8 else 1
    output = []
    for frame in frames:
        source_world = world_matrices(frame["transforms"], source_parents)
        local = copy.deepcopy(rest); posed = [None]*len(nodes)
        def visit(i):
            if posed[i] is not None: return posed[i]
            parent = parents[i]; parent_world = visit(parent) if parent>=0 else np.eye(4)
            if i in mapped:
                j = mapped[i]
                desired = rotation(source_world[j]) @ rotation(bind_source[j]).T @ rotation(target_world[i])
                local[i][1] = quaternion(rotation(parent_world).T @ desired).tolist()
            if i == hip_index:
                position = target_world[i][:3,3].copy()
                position[1] += (source_world[source_hip][1,3]-bind_source[source_hip][1,3])*ratio
                local[i][0] = (np.linalg.inv(parent_world) @ np.r_[position,1])[:3].tolist()
            posed[i] = parent_world @ trs_matrix(local[i]); return posed[i]
        for i in range(len(nodes)): visit(i)
        output.append(local)
    seam = max(float(np.degrees(np.arccos(np.clip(abs(np.dot(output[0][i][1],output[-1][i][1])),0,1)))*2) for i in mapped)
    # A short correction window retains every sampled frame and closes loop endpoints.
    if loop:
        duration = times[-1]
        for f,time in enumerate(times):
            weight = np.clip((time-(duration-min(.15,duration*.15)))/min(.15,duration*.15),0,1)
            if weight<=0: continue
            for i in mapped:
                a=np.array(output[f][i][1]); b=np.array(output[0][i][1]); b*=1 if np.dot(a,b)>=0 else -1
                q=a*(1-weight)+b*weight; output[f][i][1]=(q/np.linalg.norm(q)).tolist()
            a=np.array(output[f][hip_index][0]); b=np.array(output[0][hip_index][0]); output[f][hip_index][0]=(a*(1-weight)+b*weight).tolist()
    return output, {"mappedJoints": len(mapped), "frames": len(frames), "duration": float(times[-1]),
                    "sampleRate": source["clips"][0].get("sampleRate",30), "translationScale": ratio,
                    "sourceSeamMaximumDegrees": seam, "loopCorrectionSeconds": min(.15,float(times[-1])*.15) if loop else 0,
                    "horizontalRootMotion": "in-place", "mapping": mapping}


def read_glb(path):
    data=path.read_bytes()
    if struct.unpack_from('<III',data)[:2]!=(0x46546c67,2): raise ValueError("Expected GLB 2")
    chunks=[]; pos=12
    while pos<len(data):
        size,kind=struct.unpack_from('<II',data,pos); chunks.append((kind,data[pos+8:pos+8+size])); pos+=8+size
    return json.loads(chunks[0][1]), chunks


def validate_weights(values):
    values = np.array(values, dtype=float)
    if not np.isfinite(values).all() or np.any(values < 0): raise ValueError("Invalid skin weights")
    sums = values.sum(axis=1); missing = sums <= 1e-8
    if missing.any(): raise ValueError('Zero skin weights require restoration from the source asset')
    if not np.allclose(sums,1,atol=.02): raise ValueError('Skin weights must be normalized')
    return int(len(values))


def main():
    parser=argparse.ArgumentParser(__doc__); parser.add_argument('--target',type=Path,required=True)
    parser.add_argument('--sampler',type=Path,required=True); parser.add_argument('--idle',type=Path,required=True)
    parser.add_argument('--walk',type=Path,required=True); parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--mapping',type=Path); parser.add_argument('--hips',default='Bip001_Pelvis')
    args=parser.parse_args()
    if args.output.exists(): raise FileExistsError(args.output)
    document,chunks=read_glb(args.target); binary=bytearray(chunks[1][1]); original=bytes(binary)
    mapping=json.loads(args.mapping.read_text(encoding='utf-8')) if args.mapping else bip001_mapping()
    report={"targetSha256":hashlib.sha256(args.target.read_bytes()).hexdigest(),"clips":[]}
    def accessor(values,kind):
        array=np.array(values,dtype='<f4'); binary.extend(b'\0'*(-len(binary)%4)); offset=len(binary); binary.extend(array.tobytes())
        view=len(document['bufferViews']); document['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':array.nbytes})
        index=len(document['accessors']); record={'bufferView':view,'componentType':5126,'count':len(array),'type':kind}
        if kind=='SCALAR': record.update(min=[float(array.min())],max=[float(array.max())])
        document['accessors'].append(record); return index
    document['animations']=[]
    validated = 0
    for mesh in document['meshes']:
        for primitive in mesh['primitives']:
            attributes = primitive['attributes']
            if 'WEIGHTS_0' not in attributes: continue
            entry = document['accessors'][attributes['WEIGHTS_0']]; view = document['bufferViews'][entry['bufferView']]
            dtype = {5126:'<f4',5123:'<u2',5121:'u1'}[entry['componentType']]
            stride = view.get('byteStride', np.dtype(dtype).itemsize*4)
            values = np.ndarray((entry['count'],4),dtype=dtype,buffer=original,
                offset=view.get('byteOffset',0)+entry.get('byteOffset',0),strides=(stride,np.dtype(dtype).itemsize)).astype(float)
            if entry.get('normalized'): values /= {5121:255,5123:65535}[entry['componentType']]
            validated += validate_weights(values)
    report['validatedSkinVertices'] = validated
    for name,path in [('idle',args.idle),('walk',args.walk)]:
        sampled=subprocess.run([str(args.sampler.resolve()),str(path.resolve())],capture_output=True,check=True)
        source=json.loads(sampled.stdout); frames,details=retarget_samples(document,source,mapping,args.hips)
        times=accessor([f['time'] for f in source['clips'][0]['frames']],'SCALAR'); animation={'name':name,'samplers':[],'channels':[]}
        for i,node in enumerate(document['nodes']):
            if node.get('name') not in mapping: continue
            for track,slot,kind in [('rotation',1,'VEC4')]+([('translation',0,'VEC3')] if node.get('name')==args.hips else []):
                values=[frame[i][slot] for frame in frames]
                if slot==1:
                    for f in range(1,len(values)):
                        if np.dot(values[f-1],values[f])<0: values[f]=(-np.array(values[f])).tolist()
                animation['channels'].append({'sampler':len(animation['samplers']),'target':{'node':i,'path':track}})
                animation['samplers'].append({'input':times,'output':accessor(values,kind),'interpolation':'LINEAR'})
        document['animations'].append(animation); details.update(name=name,sourceSha256=hashlib.sha256(path.read_bytes()).hexdigest()); report['clips'].append(details)
    assert bytes(binary[:len(original)])==original
    document['buffers'][0]['byteLength']=len(binary); document['asset'].setdefault('extras',{})['azureAnimationRetarget']=report
    payload=json.dumps(document,separators=(',',':')).encode(); payload+=b' '*(-len(payload)%4); binary.extend(b'\0'*(-len(binary)%4))
    content=struct.pack('<II',len(payload),0x4e4f534a)+payload+struct.pack('<II',len(binary),0x004e4942)+binary
    for kind,chunk in chunks[2:]: content+=struct.pack('<II',len(chunk),kind)+chunk
    args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_bytes(struct.pack('<III',0x46546c67,2,len(content)+12)+content)
    report['outputSha256']=hashlib.sha256(args.output.read_bytes()).hexdigest(); args.output.with_suffix('.retarget.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report))


if __name__=='__main__': main()
