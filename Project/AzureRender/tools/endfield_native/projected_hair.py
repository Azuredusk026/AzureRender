"""CPU reference for projected, two-dimensional strand highlight validation.

The caller supplies the half vector, decoded independent highlight normal,
source parameter texture, object frame and sampled linear ramp. Signed shifts
are ``2 * source_anisotropy_value - 1``. Light color, visibility and intensity
scales outside the material response belong to the caller.
"""
from dataclasses import dataclass
import math


@dataclass(frozen=True)
class Coordinates:
    uv: tuple
    edge_fade: float
    secondary_lobe: float


@dataclass(frozen=True)
class Energy:
    q: tuple
    primary: tuple
    secondary: tuple


def _dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def _normal(a, fallback):
    length2 = _dot(a, a)
    return tuple(x / math.sqrt(length2) for x in a) if length2 > 1e-12 else fallback


def _sine(projection):
    return max(math.sqrt(max(1 - projection * projection, 0)), .0001)


def virtual_half_vector(*, view, light, object_x, object_y, object_z, blend):
    """Main-light half vector from captured event 950 instructions 1175-1196."""
    v=_normal(view,(0,0,1))
    local=(_dot(v,object_x),.5+blend*(light[1]-.5),_dot(v,object_z))
    virtual=tuple(2*(object_x[i]*local[0]+object_y[i]*local[1]+object_z[i]*local[2])+blend*light[i] for i in range(3))
    virtual=_normal(virtual,(0,0,1))
    half=tuple(a+b for a,b in zip(virtual,v))
    inverse=1/math.sqrt(max(_dot(half,half),2**-14))
    return tuple(value*inverse for value in half)


def project_coordinates(*, half_vector, highlight_normal, view, source_parameters,
                        tangent, object_x, object_y, object_z, primary_shift,
                        secondary_shift, range2, edge_fade_power, direction_x):
    """Evaluate coordinates and lobe geometry without sampling the 2D ramp.

    Near-zero vectors use fixed fallbacks. A zero projected normal or view has
    zero edge visibility. These are deterministic host protections for source
    cases whose unguarded reciprocal square root is undefined.
    """
    n = _normal(highlight_normal, (0, 0, 1))
    v = _normal(view, (0, 0, 1))
    h = _normal(half_vector, (0, 0, 1))
    axis = _normal(tuple(x * direction_x + y for x, y in zip(object_x, object_y)),
                   (0, 1, 0))
    object_tangent = _cross(n, axis)
    blend = tuple(a + source_parameters[0] * (b - a)
                  for a, b in zip(object_tangent, tangent[:3]))
    handedness = 1 + source_parameters[0] * (tangent[3] - 1)
    strand = tuple(value * handedness for value in _cross(n, blend))
    fallback = _normal(tangent[:3], (1, 0, 0))
    primary = _normal(tuple(s + x * primary_shift for s, x in zip(strand, n)), fallback)
    secondary = _normal(tuple(s + x * secondary_shift for s, x in zip(strand, n)), fallback)
    normal_projection = (_dot(n, object_x), _dot(n, object_z))
    view_projection = (_dot(v, object_x), _dot(v, object_z))
    np2, vp2 = _dot(normal_projection, normal_projection), _dot(view_projection, view_projection)
    edge = 0.0
    if np2 > 1e-12 and vp2 > 1e-12:
        cosine = min(max(_dot(normal_projection, view_projection) / math.sqrt(np2 * vp2), 0), 1)
        edge = cosine ** edge_fade_power
    projection = min(max(_dot(primary, h), -1), 1)
    u = min(max(source_parameters[1] * _sine(projection) ** 200, 0), 1)
    exponent = math.trunc(max(1 - range2, 0) * 200)
    lobe2 = _sine(min(max(_dot(secondary, h), -1), 1)) ** exponent
    return Coordinates((u, edge * edge if projection > 0 else 0.0), edge, lobe2)


def evaluate_energy(coordinates, ramp_sample, source_parameters, intensity, secondary_color):
    """Consume a linear RGB ramp sample at the full two-dimensional UV."""
    q = tuple(value * coordinates.uv[0] * coordinates.edge_fade for value in ramp_sample)
    primary = tuple(value * source_parameters[1] * .04 * intensity * 5 for value in q)
    secondary_scale = (source_parameters[3] * coordinates.edge_fade
                       * coordinates.secondary_lobe * (1 - max(q)))
    return Energy(q, primary, tuple(value * secondary_scale for value in secondary_color))


def line_response(*, strand, normal, half_vector, u, frequency, strand_shift,
                  range1, depth, alternation, line_sample, parameter_g, q, saturation):
    """Captured event 950 instructions 1219-1248; indirect factor and saturation."""
    shifted=_normal(tuple(s+n*strand_shift for s,n in zip(strand,normal)),(1,0,0))
    lobe=_sine(_dot(shifted,half_vector))**math.trunc(max(1-range1,0)*200)
    phase=u*frequency-math.floor(u*frequency)
    alternating=1.0 if phase>.5 else 0.0
    weight=alternating+alternation*(1-line_sample-alternating)
    base=1+depth*(weight-1)
    protected=base+(1-base)*max(q)
    factor=1+parameter_g*(protected-1)*lobe
    return factor,saturation+(1-saturation)*factor
