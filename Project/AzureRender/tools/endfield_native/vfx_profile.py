"""Select a public surface model from an audited native VFX identity."""
import math


UNLIT_VFX_SHADER = (
    'CAB-9fe1ba4f3ce8edb2f54eb3e35709a336', -1430105248647086886)


def unlit_vfx_profile(record):
    shader = record.get('Shader', {})
    identity = (shader.get('Source'), shader.get('PathId'))
    if (identity != UNLIT_VFX_SHADER
            or shader.get('Name') != 'HGRP/Effect/VFXBaseV2'):
        raise ValueError('Unverified VFX shader identity')
    switches = record.get('Floats', {})
    for name in ('_UseLighting', '_UseForceDynamicLighting', '_EnvLightStrength'):
        value = switches.get(name)
        if (type(value) not in (int, float) or not math.isfinite(value)
                or value != 0):
            raise ValueError('Unlit VFX requires explicit zero ' + name)
    return {'schemaVersion': 2, 'class': 'emissive',
            'features': ['emissive-mask', 'unlit-surface'],
            'surface': {'schemaVersion': 1, 'lighting': 'unlit'}}
