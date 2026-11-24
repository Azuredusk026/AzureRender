"""Reference-correlated amplitude for aligned linear-color image sequences."""
import numpy as np


def correlated_detail(candidate, reference, indices=slice(None), highpass=False):
    a,b=(np.asarray(value) for value in (candidate,reference))
    if a.shape!=b.shape or a.ndim!=4 or not all(a.shape):
        raise ValueError('Detail inputs must have matching nonempty frame/height/width/channel dimensions')
    if any(not np.issubdtype(value.dtype,np.floating) or not np.isfinite(value).all() for value in (a,b)):
        raise ValueError('Detail inputs must contain finite linear floating-point colors')
    a,b=a[indices],b[indices]
    if a.ndim!=4 or not a.shape[0]:
        raise ValueError('Detail measurement requires a nonempty frame selection')
    if highpass:
        def residual(value):
            height,width=value.shape[1:3]
            padded=np.pad(value,((0,0),(1,1),(1,1),(0,0)),mode='edge')
            average=sum(padded[:,y:y+height,x:x+width] for y in range(3) for x in range(3))/9
            return value-average
        a,b=residual(a),residual(b)
    a=a-a.mean((1,2),keepdims=True)
    b=b-b.mean((1,2),keepdims=True)
    energy=(b*b).sum((1,2,3))
    if energy.min()<=1e-8:
        raise ValueError('The detail reference must contain resolvable structure')
    return ((a*b).sum((1,2,3))/energy).tolist()
