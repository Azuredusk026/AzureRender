"""Select an explicit native draw interval without inferring camera semantics.

The source threshold is a squared scalar. Its mapping from camera and bounds
requires a separate consumer investigation. Highest selects the greatest
authored interval and rejects overlapping independently authored groups.
"""
import math


def select_draw_indices(pairs, *, policy='metric', metric_squared=0.0):
    if policy not in ('metric', 'highest'):
        raise ValueError('unsupported source LOD policy')
    if (type(metric_squared) not in (int, float) or not math.isfinite(metric_squared)
            or metric_squared < 0):
        raise ValueError('source LOD metric must be finite and nonnegative')
    visible = []
    for index, pair in enumerate(pairs):
        if not isinstance(pair, dict) or not isinstance(pair.get('mesh'), str) or not pair['mesh']:
            raise ValueError('invalid source mesh identity')
        draw = pair.get('draw')
        if not isinstance(draw, dict):raise ValueError('missing source draw')
        lower, upper = draw.get('lod_min_squared'), draw.get('lod_max_squared')
        if (any(type(x) not in (int, float) or not math.isfinite(x) for x in (lower, upper))
                or upper <= lower or type(draw.get('submesh')) is not int or draw['submesh'] < 0
                or type(draw.get('flags')) is not int or not 0 <= draw['flags'] <= 0xffffffff):
            raise ValueError('invalid source draw interval or identity')
        # Pinned reader's visible mesh contract defines bit 1024 as shadow-only.
        if not draw['flags'] & 1024:
            visible.append((index, lower, upper))
    if not visible:raise ValueError('no visible source draws')
    if policy == 'metric':
        selected = [i for i, lower, upper in visible if lower <= metric_squared < upper]
    else:
        upper, lower = max((upper, lower) for _, lower, upper in visible)
        if any((hi, lo) != (upper, lower) and hi > lower for _, lo, hi in visible):
            raise ValueError('overlapping authored LOD intervals require independent renderer groups')
        selected = [i for i, lo, hi in visible if (hi, lo) == (upper, lower)]
    if not selected:raise ValueError('source LOD policy selects no visible draws')
    return selected
