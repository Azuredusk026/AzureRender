"""Material-only eye reference with explicit upstream linear inputs.

Circle/parallax, the two radial normals, RGBA Matcap and direct terms follow
the audited eye variant. Native NPR illumination and runtime CP0/CP13 values
belong to the caller. This module does not infer those values or native color
upload conversion from serialized material colors.
"""
from dataclasses import dataclass
import math


@dataclass(frozen=True)
class EyeCoordinates:
    frac_uv: tuple
    white_mask: float
    parallax_uv: tuple
    lighting_normal: tuple
    matcap_uv: tuple


@dataclass(frozen=True)
class EyeTerms:
    diffuse_multiplier: tuple
    matcap: tuple
    direct: tuple


def _dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def _cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def _unit(a):
    square = _dot(a, a)
    return tuple(v/math.sqrt(square) for v in a) if square > 1e-24 else (0., 0., 1.)


def surface_coordinates(*, uv, view=(0, 0, 1), normal=(0, 0, 1),
                        tangent=(1, 0, 0, 1), parallax_scale=.15,
                        matcap_normal_scale=1,
                        view_rotation=((1, 0, 0), (0, 1, 0), (0, 0, 1))):
    """Consume transformed original UV and an interpolated world frame.

    View rotation is a row-major world-to-view rotation. The original frame
    lengths are retained. Near-zero vectors use fixed host fallbacks; source
    reciprocal-square-root behavior at singular inputs is not established.
    """
    frac = tuple(v-math.floor(v) for v in uv)
    radius2 = sum((v-.5)**2 for v in frac)
    white = float(radius2 >= .25)
    weight = min(max((.25-radius2)*5, 0), 1)
    weight *= weight*(3-2*weight)
    cross_nt = _cross(normal, tangent[:3])
    parallax_b = tuple(v*(1 if tangent[3] > 0 else -1) for v in cross_nt)
    normal_length = max(math.sqrt(_dot(normal, normal)), 1e-12)
    view_ts = _unit(tuple(_dot(axis, view)/normal_length
                         for axis in (tangent[:3], parallax_b, normal)))
    parallax = (uv[0]-view_ts[0]*parallax_scale*weight,
                uv[1]-view_ts[1]*parallax_scale*.25*weight)
    x, y = (v*2-1 for v in frac)
    mc = (-x*matcap_normal_scale, -y*matcap_normal_scale,
          math.sqrt(max(1-x*x-y*y, 0)))
    local_light = (mc[0]*-.125*(1-white), mc[1]*-.125*(1-white),
                   mc[2]*(1-white)+white)
    bitangent = tuple(v*tangent[3] for v in cross_nt)
    def world(v):
        return tuple(tangent[i]*v[0]+bitangent[i]*v[1]+normal[i]*v[2] for i in range(3))
    lighting_normal = _unit(world(local_light))
    world_mc = world(mc)
    view_mc = _unit(tuple(_dot(row, world_mc) for row in view_rotation))
    return EyeCoordinates(frac, white, parallax, lighting_normal,
                          (view_mc[0]*.5+.5, view_mc[1]*.5+.5))


def decode_srgb_rgba(sample):
    """Decode encoded RGB once; alpha stays linear and unmodified."""
    rgb = tuple(v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4 for v in sample[:3])
    return rgb+(sample[3],)


def evaluate_terms(*, base_sample, base_color=(1, 1, 1, 1), white_mask,
                   highlight_color, scattering_color, matcap_sample, matcap_color,
                   view_ramp_alpha, light_ramp_alpha, shadow_select, shadow_floor,
                   npr_illumination, direct_weights, alpha_premultiply=0):
    """Consume linear samples and explicit globals, preserving HDR energy.

    The diffuse multiplier is a material term, not complete source diffuse.
    Matcap multiplies upstream NPR illumination independently from base color.
    Direct energy uses explicit CP13 analogue weights and may remain visible
    when upstream illumination is zero.
    """
    base = tuple(s*c for s, c in zip(base_sample, base_color))
    highlight = tuple(white_mask*c for c in highlight_color)
    scattering = tuple(base[3]*c for c in scattering_color)
    multiplier = tuple((1+white_mask*(h-1))*(1+base[3]*(s-1))
                       for h, s in zip(highlight_color, scattering_color))
    ramp = view_ramp_alpha+shadow_select*(min(light_ramp_alpha, 1)-view_ramp_alpha)
    intensity = (.5*ramp+.5)*(ramp*(1-shadow_floor)+shadow_floor)
    matcap = tuple((m*matcap_color[3]+matcap_sample[3]*c)*intensity*light
                   for m, c, light in zip(matcap_sample[:3], matcap_color[:3], npr_illumination))
    factor = 1+alpha_premultiply*(base[3]-1)
    direct = tuple((a*direct_weights[0]+h*direct_weights[1]+s*direct_weights[2])*factor
                   for a, h, s in zip(base[:3], highlight, scattering))
    return EyeTerms(multiplier, matcap, direct)
