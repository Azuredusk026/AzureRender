"""Explicit color-parameter upload for the verified tri-channel surface branch."""
import copy
import math

SURFACE_COLORS=('_BaseColor','_MaskAlbedoR','_MaskAlbedoG','_MaskAlbedoB')


def surface_color_record(record, encoding='gpu-linear'):
    if encoding not in ('gpu-linear','authored-srgb'):
        raise ValueError('Surface colors require an explicit supported encoding')
    if record.get('SurfaceColorEncoding',encoding)!=encoding:
        raise ValueError('Surface color encoding contradicts the supplied record')
    result=copy.deepcopy(record)
    colors=result.setdefault('Colors',{})
    for name in SURFACE_COLORS:
        if name not in colors:continue
        values=colors[name]
        if (not isinstance(values,(list,tuple)) or len(values)!=4
                or any(type(v) not in (int,float) or not math.isfinite(v) for v in values)):
            raise ValueError('Surface color requires four finite numbers: '+name)
        converted=list(values)
        if encoding=='authored-srgb':
            try:converted[:3]=[v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in values[:3]]
            except OverflowError as error:raise ValueError('Surface color conversion overflow: '+name) from error
        if not all(math.isfinite(v) for v in converted):raise ValueError('Nonfinite surface color: '+name)
        colors[name]=converted
    result['SurfaceColorEncoding']='gpu-linear'
    return result
