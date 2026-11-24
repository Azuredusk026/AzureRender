"""Recover a pinhole camera from metric landmarks with independent holdouts.

Uses NumPy and normalized DLT. Image coordinates are pixels, origin top left.
World coordinates retain the caller's metric frame. The recovered camera uses
image-right X, image-down Y and forward Z; its world axes are explicit below.
The projection fit alone does not qualify a game reference for visual scoring.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path

import numpy as np


def _finite(value):
    return type(value) in (int,float) and math.isfinite(value)


def _normalize(points):
    center=points.mean(axis=0)
    radius=np.sqrt(np.mean(np.sum((points-center)**2,axis=1)))
    if radius<1e-12:raise ValueError('degenerate landmark distribution')
    scale=math.sqrt(points.shape[1])/radius
    transform=np.eye(points.shape[1]+1)
    transform[:-1,:-1]*=scale
    transform[:-1,-1]=-center*scale
    return (points-center)*scale,transform


def calibrate(document):
    if not isinstance(document,dict) or document.get('format')!='RenderCameraLandmarks/1':
        raise ValueError('unsupported landmark format')
    resolution=document.get('resolution')
    if not isinstance(resolution,list) or len(resolution)!=2 or any(type(x) is not int or x<2 for x in resolution):
        raise ValueError('invalid resolution')
    threshold=document.get('maximum_error_pixels')
    if not _finite(threshold) or threshold<=0:
        raise ValueError('maximum_error_pixels must be finite and positive')
    rows=document.get('landmarks')
    if not isinstance(rows,list):raise ValueError('landmarks must be a list')
    identities=set()
    for row in rows:
        if not isinstance(row,dict):raise ValueError('invalid landmark')
        identity=row.get('id')
        if not isinstance(identity,str) or not identity or identity in identities:
            raise ValueError('missing or duplicate landmark identity')
        identities.add(identity)
        if row.get('role') not in ('fit','validation'):raise ValueError('invalid landmark role')
        for key,count in [('world',3),('pixel',2)]:
            value=row.get(key)
            if not isinstance(value,list) or len(value)!=count or not all(map(_finite,value)):
                raise ValueError('landmark '+key+' must contain finite components')
    fitting=[r for r in rows if r['role']=='fit']
    if len(fitting)<6 or len(rows)-len(fitting)<2:
        raise ValueError('at least six fit and two validation landmarks are required')
    world=np.asarray([r['world'] for r in fitting],dtype=float)
    pixels=np.asarray([r['pixel'] for r in fitting],dtype=float)
    xyz,world_transform=_normalize(world)
    uv,image_transform=_normalize(pixels)
    geometry=np.linalg.svd(xyz,compute_uv=False)
    if geometry[-1]<geometry[0]*1e-6:
        raise ValueError('degenerate nonvolumetric landmark geometry')
    equations=[]
    for point,(u,v) in zip(xyz,uv):
        h=np.append(point,1.)
        equations.extend([np.r_[h,np.zeros(4),-u*h],np.r_[np.zeros(4),h,-v*h]])
    _,singular,vt=np.linalg.svd(np.asarray(equations),full_matrices=False)
    if singular[-2]<singular[0]*1e-10:
        raise ValueError('degenerate projection equations')
    projection=np.linalg.inv(image_transform)@vt[-1].reshape(3,4)@world_transform
    if np.linalg.det(projection[:,:3])<0:projection=-projection
    q,r=np.linalg.qr(np.flipud(projection[:,:3]).T)
    intrinsic=np.fliplr(np.flipud(r.T));rotation=np.flipud(q.T)
    signs=np.diag(np.where(np.diag(intrinsic)<0,-1.,1.))
    intrinsic=intrinsic@signs;rotation=signs@rotation
    if np.linalg.det(rotation)<0 or abs(intrinsic[2,2])<1e-12:
        raise ValueError('invalid recovered camera orientation')
    scale=intrinsic[2,2];intrinsic/=scale;projection/=scale
    translation=np.linalg.solve(intrinsic,projection[:,3])
    position=-rotation.T@translation
    errors={'fit':[],'validation':[]};reprojected=[]
    for row in rows:
        camera=rotation@np.asarray(row['world'])+translation
        if camera[2]<=1e-8:raise ValueError('landmark behind recovered camera')
        homogeneous=intrinsic@camera;pixel=homogeneous[:2]/homogeneous[2]
        error=float(np.linalg.norm(pixel-np.asarray(row['pixel'])))
        errors[row['role']].append(error)
        reprojected.append(dict(id=row['id'],role=row['role'],pixel=pixel.tolist(),error_pixels=error))
    width,height=resolution
    fx,fy=intrinsic[0,0],intrinsic[1,1]
    principal_error=float(np.linalg.norm(intrinsic[:2,2]-np.array([width/2,height/2])))
    # A general projection can fit even when a centered, square-pixel engine
    # camera cannot represent it. Report those gates independently.
    representable=bool(fx>0 and fy>0 and abs(fx/fy-1)<=.005 and
                       abs(intrinsic[0,1])<=fy*.001 and principal_error<=2)
    return dict(format='RenderCameraCalibration/1',resolution=resolution,
        method='normalized DLT; validation landmarks excluded from fitting',
        coordinate_convention='world units preserved; camera X right, Y down, Z forward',
        intrinsics=intrinsic.tolist(),world_to_camera_rotation=rotation.tolist(),
        world_to_camera_translation=translation.tolist(),projection=projection.tolist(),
        camera=dict(position=position.tolist(),right=rotation[0].tolist(),
            up=(-rotation[1]).tolist(),forward=rotation[2].tolist(),
            fov_y_degrees=math.degrees(2*math.atan(height/(2*fy)))),
        fit_maximum_error_pixels=max(errors['fit']),
        validation_maximum_error_pixels=max(errors['validation']),
        maximum_error_pixels=threshold,principal_point_error_pixels=principal_error,
        centered_square_pixel_camera=representable,
        projection_accepted=bool(representable and max(errors['fit']+errors['validation'])<=threshold),
        reference_qualified=False,landmarks=reprojected)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('landmarks',type=Path)
    parser.add_argument('--image',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    document=json.loads(args.landmarks.read_text(encoding='utf-8-sig'))
    result=calibrate(document)
    from PIL import Image
    with Image.open(args.image) as image:
        if list(image.size)!=document['resolution']:raise ValueError('image resolution mismatch')
    result['inputs']=[dict(path=str(p.resolve()),sha256=hashlib.sha256(p.read_bytes()).hexdigest())
                      for p in [args.landmarks,args.image]]
    with args.output.open('x',encoding='utf-8') as stream:
        json.dump(result,stream,indent=2,allow_nan=False)
    print(json.dumps({k:result[k] for k in ['projection_accepted','fit_maximum_error_pixels',
        'validation_maximum_error_pixels','reference_qualified']}))


if __name__=='__main__':main()
