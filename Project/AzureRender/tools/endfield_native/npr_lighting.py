"""Decode a caller-confirmed NPR constant layout into public scene inputs.

The caller identifies the draw and its ambient/main-color rows. Source object
and global blending is preserved; constants are already linear. World output
uses the converter's reflected-X convention unless explicitly disabled.
"""
import math


def make_zero_global_rim_override(global_rows,*,rim_row):
    """Author the existing rim parameter only after confirming zero native global radiance."""
    value=global_rows.get(rim_row)
    if not isinstance(value,(list,tuple)) or len(value)!=4 or any(type(v) not in (int,float) or not math.isfinite(v) or v<0 or v>64 for v in value):
        raise ValueError('Missing or invalid native global rim row')
    if any(v*value[3]!=0 for v in value[:3]):
        raise ValueError('Nonzero native global rim requires its explicit consumer')
    return dict(schemaVersion=2,mode='parameters',typeId='azure.stylized',parameters={'rimAmount':0},textures={})


def decode_lighting(global_rows,object_rows,*,ambient_row,main_color_row,reflect_x=True):
    def row(rows,index):
        value=rows.get(index)
        if not isinstance(value,(list,tuple)) or len(value)!=4 or any(type(v) not in (int,float) or not math.isfinite(v) for v in value):
            raise ValueError('Missing or invalid NPR constant row: '+str(index))
        return value
    def unit(value):
        length=math.sqrt(sum(v*v for v in value))
        if length<1e-8:raise ValueError('NPR direction must be nonzero')
        return [v/length for v in value]
    def bounded(value,minimum,maximum):
        if not minimum<=value<=maximum:raise ValueError('Captured NPR parameter outside supported domain')
        return value
    gains=row(global_rows,110);modes=row(global_rows,111);blend=row(global_rows,122)
    ambient=row(global_rows,ambient_row);global_color=row(global_rows,main_color_row)
    global_intensity=row(global_rows,115)[3];object_color=row(object_rows,3)
    ambient_direction=unit(row(global_rows,116)[:3]);direction_parameters=row(global_rows,117)
    global_direction=row(global_rows,121);object_direction=row(object_rows,0)
    for v in modes:bounded(v,0,1)
    for v in blend:bounded(v,0,1)
    ambient_base=bounded(row(global_rows,29)[0],0,1)
    ambient_gain=bounded(row(global_rows,27)[0],0,16)
    ambient_weight=bounded((ambient_base+blend[3]*(1-2*ambient_base))*ambient_gain,0,16)
    incoming=unit([global_direction[i]*modes[3]-object_direction[i]*(1-modes[3]) for i in range(3)])
    if reflect_x:incoming[0]*=-1;ambient_direction[0]*=-1
    color=[object_color[i]+blend[1]*(global_color[i]-object_color[i]) for i in range(3)]
    intensity=object_color[3]+blend[3]*(global_intensity-object_color[3])
    for v in list(ambient[:3])+color:bounded(v,0,64)
    bounded(intensity,0,65536);bounded(direction_parameters[0],-1,1)
    for v in (direction_parameters[1],direction_parameters[2],gains[1],gains[3]):bounded(v,0,16)
    return {'settings':{'schemaVersion':1,'enabled':True,'ambientColor':list(ambient[:3]),
        'ambientDirection':ambient_direction,'directionBias':direction_parameters[0],
        'directionGain':direction_parameters[1],'directionFloor':direction_parameters[2],
        'outdoorGain':gains[1],'shadowAmbientGain':gains[3],'ambientWeight':ambient_weight,'indoorMix':modes[0],
        'litAmbientMix':modes[1],'mainColorMix':blend[1],'shadowOverride':modes[2],
        'useSceneIrradiance':modes[1]<.5},
        'mainLight':{'incomingDirection':incoming,'color':color,'intensity':intensity},
        'materialShadowScale':bounded(gains[2],0,64),
        'sourceChoices':{'ambientRow':ambient_row,'mainColorRow':main_color_row,
            'directionBlend':modes[3],'colorBlend':blend[1],'intensityBlend':blend[3],
            'ambientWeightRows':[27,29,122]},
        'unverifiedInputs':(['volume-modulated ambient weight'] if modes[1]<.5 else [])
            +['material-specific color inputs','native grading and unclipped HDR output']}


def decode_surface_lighting(global_rows,object_rows,*,ambient_row,main_color_row):
    """Author explicit material colors from caller-confirmed capture rows."""
    decoded=decode_lighting(global_rows,object_rows,ambient_row=ambient_row,main_color_row=main_color_row)
    return dict(schemaVersion=1,ambientColor=decoded['settings']['ambientColor'],mainColor=decoded['mainLight']['color'])


def decode_projected_hair_lighting(global_rows,object_rows,material_rows,*,ambient_row,main_color_row,shadow_color_row):
    """Join caller-confirmed material Z/W shadow controls with captured light colors."""
    decoded=decode_lighting(global_rows,object_rows,ambient_row=ambient_row,main_color_row=main_color_row)
    row=material_rows.get(shadow_color_row)
    if not isinstance(row,(list,tuple)) or len(row)!=4 or any(type(v) not in (int,float) or not math.isfinite(v) for v in row):
        raise ValueError('Missing or invalid projected hair shadow-color row')
    brightness,saturation=row[2:]
    scale=decoded['materialShadowScale']
    if any(not 0<=v<=16 for v in (brightness,saturation,scale)):
        raise ValueError('Projected hair shadow parameters outside supported domain')
    return dict(schemaVersion=1,shadowBrightness=brightness,shadowSaturation=saturation,shadowScale=scale,
        ambientColor=decoded['settings']['ambientColor'],mainColor=decoded['mainLight']['color'])
