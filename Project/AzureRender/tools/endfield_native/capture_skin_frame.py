"""Compare a captured NPR reserved frame with explicitly ordered source bones.

The caller supplies native Unity inverse bind matrices. Converter geometry
reflects X and must be converted back before calling. No bone identity is
inferred from proximity; position and full-matrix residuals are separate.
"""
import struct
import copy
import numpy as np


def compare_skin_frame(instance_bytes, palette_bytes, inverse_binds, instance_index=0):
    if type(instance_index) is not int or instance_index < 0:
        raise ValueError('Invalid instance index')
    offset = instance_index * 256
    if len(instance_bytes) < offset + 256 or len(palette_bytes) % 16:
        raise ValueError('Incomplete instance or palette range')
    flags, = struct.unpack_from('<I', instance_bytes, offset + 76)
    if not flags & 16:
        raise ValueError('Draw does not select a reserved skin frame')
    influences=flags&~48
    if influences not in (1,2,4):
        raise ValueError('Draw does not select a supported active skin palette')
    base, = struct.unpack_from('<I', instance_bytes, offset + 80)
    count = len(inverse_binds)
    if not count or (base + 3 + 3 * count) * 16 > len(palette_bytes):
        raise ValueError('Palette does not cover the declared bones')
    model = np.array(struct.unpack_from('<16f', instance_bytes, offset)).reshape(4, 4).T
    rows = np.frombuffer(palette_bytes, dtype='<f4').reshape(-1, 4)
    frame = np.eye(4); frame[:3] = rows[base:base + 3]
    if not np.isfinite(model).all() or not np.isfinite(frame).all():
        raise ValueError('Nonfinite captured matrix')
    comparisons = []
    for index, value in enumerate(inverse_binds):
        bind = np.asarray(value, dtype=float)
        if bind.shape != (4, 4) or not np.isfinite(bind).all():
            raise ValueError('Invalid inverse bind matrix')
        if not np.allclose(bind[3], [0, 0, 0, 1], atol=1e-6, rtol=0):
            raise ValueError('Inverse bind is not affine; check matrix convention')
        skin = np.eye(4); first = base + 3 + 3 * index
        skin[:3] = rows[first:first + 3]
        if not np.isfinite(skin).all():
            raise ValueError('Nonfinite captured bone')
        try:
            world = model @ skin @ np.linalg.inv(bind)
        except np.linalg.LinAlgError as error:
            raise ValueError('Noninvertible inverse bind matrix') from error
        if not np.isfinite(world).all():
            raise ValueError('Nonfinite reconstructed bone')
        comparisons.append({'index': index, 'world': world.tolist(),
            'position_error': float(np.linalg.norm(world[:3, 3] - frame[:3, 3])),
            'matrix_error': float(np.max(np.abs(world[:3] - frame[:3])))})
    return {'format': 'NativeReservedSkinFrameComparison/1', 'instance_index': instance_index,
        'flags': flags, 'palette_row': base, 'reserved_frame': frame.tolist(),
        'origin': frame[:3, 3].tolist(), 'model_origin': model[:3, 3].tolist(),
        'bones': comparisons, 'scope': 'One captured pose; caller verifies source mesh and bone order'}


def freeze_gltf_pose(document, world_by_name, *, freeze_animations=False):
    """Create a diagnostic pose using explicit named worlds in glTF coordinates.

    Geometry and skin streams remain unchanged. Animation removal requires an
    explicit request; the caller retains the animated production source.
    """
    from retarget_animation import trs_matrix
    if document.get('animations') and not freeze_animations:
        raise ValueError('Captured pose requires explicit animation freeze')
    result=copy.deepcopy(document);nodes=result.get('nodes',[])
    parents=[-1]*len(nodes);names={}
    for index,node in enumerate(nodes):
        names.setdefault(node.get('name',''),[]).append(index)
        for child in node.get('children',[]):
            if type(child) is not int or not 0<=child<len(nodes) or parents[child]!=-1:
                raise ValueError('Invalid or multiply parented node')
            parents[child]=index
    def affine(value):
        matrix=np.asarray(value,dtype=float)
        if matrix.shape!=(4,4) or not np.isfinite(matrix).all() or not np.allclose(matrix[3],[0,0,0,1],atol=1e-7,rtol=0):
            raise ValueError('Invalid captured affine matrix')
        if abs(np.linalg.det(matrix[:3,:3]))<1e-12:raise ValueError('Singular captured matrix')
        return matrix
    targets={}
    for name,world in world_by_name.items():
        if not name or len(names.get(name,[]))!=1:raise ValueError('Captured pose requires unique node: '+str(name))
        targets[names[name][0]]=affine(world)
    worlds={};visiting=set()
    def visit(index):
        if index in worlds:return worlds[index]
        if index in visiting:raise ValueError('Cyclic captured pose hierarchy')
        visiting.add(index);parent=visit(parents[index]) if parents[index]>=0 else np.eye(4)
        node=nodes[index]
        if index in targets:
            world=targets[index];local=np.linalg.inv(parent)@world
            for field in ('translation','rotation','scale'):node.pop(field,None)
            node['matrix']=local.T.ravel().tolist()
        else:
            local=affine(np.asarray(node['matrix']).reshape(4,4,order='F')) if 'matrix' in node else trs_matrix((
                node.get('translation',[0,0,0]),node.get('rotation',[0,0,0,1]),node.get('scale',[1,1,1])))
            world=affine(parent@local)
        worlds[index]=world;visiting.remove(index);return world
    for index in range(len(nodes)):visit(index)
    animations=len(result.get('animations',[]))
    if freeze_animations:result.pop('animations',None)
    return result,{'format':'CapturedGltfPose/1','matchedNodes':len(targets),
        'frozenAnimations':animations if freeze_animations else 0,'geometryAndSkinPreserved':True}


def bake_skin_streams(*,positions,normals,tangents,joints,weights,matrices):
    """Evaluate the captured linear skin equation in caller-declared coordinates.

    Matrices include object and basis conversion. Direction vectors follow the
    captured vertex program's linear blend, followed by normalization. Tangent
    handedness is supplied by the caller's stream convention.
    """
    p=np.asarray(positions,dtype=float);n=np.asarray(normals,dtype=float);t=np.asarray(tangents,dtype=float)
    j=np.asarray(joints);w=np.asarray(weights,dtype=float);m=np.asarray(matrices,dtype=float)
    count=len(p)
    if p.shape!=(count,3) or n.shape!=p.shape or t.shape!=(count,4) or j.shape!=(count,4) or w.shape!=j.shape:
        raise ValueError('Skin stream dimensions disagree')
    if m.ndim!=3 or m.shape[1:]!=(4,4) or not len(m) or not np.issubdtype(j.dtype,np.integer):
        raise ValueError('Invalid skin matrices or joint indices')
    if not all(np.isfinite(a).all() for a in (p,n,t,w,m)) or (j<0).any() or (j>=len(m)).any():
        raise ValueError('Invalid skin stream values')
    if (w<0).any() or not np.allclose(w.sum(axis=1),1,atol=1e-4,rtol=0):
        raise ValueError('Skin weights must be normalized')
    if not np.allclose(m[:,3,:],[0,0,0,1],atol=1e-6,rtol=0):raise ValueError('Skin matrices must be affine')
    blended=np.einsum('ni,nijk->njk',w,m[j])
    def direction(value):
        transformed=np.einsum('nij,nj->ni',blended[:,:3,:3],value)
        lengths=np.linalg.norm(transformed,axis=1)
        if (lengths<1e-12).any():raise ValueError('Degenerate skinned direction')
        return transformed/lengths[:,None]
    return {'positions':np.einsum('nij,nj->ni',blended[:,:3,:3],p)+blended[:,:3,3],
        'normals':direction(n),'tangents':np.column_stack((direction(t[:,:3]),t[:,3]))}
