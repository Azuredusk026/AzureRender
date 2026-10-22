"""Compose explicitly supplied samples from the observed V3 irradiance path.

The source is DeferredLighting, PathID 6850169740889141214, DXBC SHA256
32a764cb2a1a9210ee1a4104dccfdf67e91a2452e42e834a73bee98115d21c37,
instructions 2284..2438 in the retained segment1-1001480 disassembly.

Inputs describe texture *results*, including every directional Alpha lane.
World position is after the source's position jitter. Sampling/filtering,
activation, globals production, optional t23 processing and final directional
strength are separate contracts. No native runtime values are inferred here.
"""
from __future__ import annotations

from collections.abc import Mapping
import math

from .irradiance import decode_amplitude


def _number(value: object, name: str) -> float:
    if type(value) not in (int, float):
        raise ValueError(f'{name} requires a finite number')
    try:
        result = float(value)
    except (OverflowError, ValueError) as exc:
        raise ValueError(f'{name} requires a finite number') from exc
    if not math.isfinite(result):
        raise ValueError(f'{name} requires a finite number')
    return result


def _vector(value: object, size: int, name: str) -> list[float]:
    if not isinstance(value, (list, tuple)) or len(value) != size:
        raise ValueError(f'{name} requires {size} components')
    return [_number(x, f'{name}[{i}]') for i, x in enumerate(value)]


def _matrix(value: object, rows: int, columns: int, name: str) -> list[list[float]]:
    if not isinstance(value, (list, tuple)) or len(value) != rows:
        raise ValueError(f'{name} requires {rows} rows')
    return [_vector(row, columns, f'{name}[{i}]') for i, row in enumerate(value)]


def _fields(value: object, required: set[str], optional: set[str], name: str) -> Mapping:
    if not isinstance(value, Mapping):
        raise ValueError(f'{name} requires a mapping')
    keys = set(value)
    if not required <= keys or keys - required - optional:
        raise ValueError(f'{name} has missing or unknown fields')
    return value


def _layer(value: object, index: int) -> tuple[list[float], list[list[float]]]:
    name = f'sampledLayers[{index}]'
    row = _fields(value, {'amplitude', 'directionalRgba'},
                  {'directionalRgba8', 'directionalAlpha'}, name)
    amplitude = _vector(row['amplitude'], 3, name + '.amplitude')
    direction = _matrix(row['directionalRgba'], 3, 4, name + '.directionalRgba')
    if any(x < 0 for x in amplitude):
        raise ValueError(name + '.amplitude must be nonnegative')
    if any(not 0 <= x <= 1 for color in direction for x in color):
        raise ValueError(name + '.directionalRgba must lie in [0,1]')
    if 'directionalAlpha' in row:
        alpha = _vector(row['directionalAlpha'], 3, name + '.directionalAlpha')
        if alpha != [color[3] for color in direction]:
            raise ValueError(name + '.directionalAlpha disagrees with directionalRgba')
    if 'directionalRgba8' in row:
        raw = row['directionalRgba8']
        if (not isinstance(raw, (list, tuple)) or len(raw) != 12
                or any(type(x) is not int or not 0 <= x <= 255 for x in raw)):
            raise ValueError(name + '.directionalRgba8 requires twelve bytes')
        if [x / 255 for x in raw] != [x for color in direction for x in color]:
            raise ValueError(name + '.directionalRgba8 disagrees with directionalRgba')
    return amplitude, direction


def decode_native_irradiance_sample(raw: object = None) -> dict:
    """Decode sixteen source bytes and preserve all directional byte/Alpha data.

    Amplitude decoding uses the existing native reader. The result can be one
    ``sampledLayers`` element at an exact source sample. Filtered texture samples
    instead supply amplitude/directionalRgba without raw-byte metadata.
    """
    if not isinstance(raw, (bytes, bytearray)) or len(raw) != 16:
        raise ValueError('Native irradiance sample requires sixteen bytes')
    source = bytes(raw)
    amplitude = decode_amplitude(source)
    rgba8 = list(source[4:])
    rgba = [[x / 255 for x in rgba8[i:i + 4]] for i in (0, 4, 8)]
    return dict(amplitude=amplitude, directionalRgba=rgba,
                directionalRgba8=rgba8, directionalAlpha=[color[3] for color in rgba])


def compose_native_irradiance(parameters: object = None) -> dict:
    """Return signed RGB coefficients, native level weights and Alpha controls.

    The complete version-1 mapping requires worldPosition, cameraPosition,
    chunkOrigin, centerScales, enabled, three sampledLayers and three explicit
    defaultCoefficients float4 rows. Disabled inputs receive identical structural
    and numeric validation. Malformed or missing data raises ValueError.

    This function returns coefficients before optional visibility processing,
    directional strength and normal evaluation; coefficients are not clamped.
    Output arrays have independent ownership.
    """
    row = _fields(parameters,
        {'version', 'worldPosition', 'cameraPosition', 'chunkOrigin',
         'centerScales', 'enabled', 'sampledLayers', 'defaultCoefficients'}, set(),
        'Irradiance composition')
    if type(row['version']) is not int or row['version'] != 1:
        raise ValueError('Unsupported irradiance composition version')
    if type(row['enabled']) is not bool:
        raise ValueError('enabled requires a boolean')
    world = _vector(row['worldPosition'], 3, 'worldPosition')
    camera = _vector(row['cameraPosition'], 3, 'cameraPosition')
    origin = _vector(row['chunkOrigin'], 3, 'chunkOrigin')
    scales = _vector(row['centerScales'], 3, 'centerScales')
    defaults = _matrix(row['defaultCoefficients'], 3, 4, 'defaultCoefficients')
    layers = row['sampledLayers']
    if not isinstance(layers, (list, tuple)) or len(layers) != 3:
        raise ValueError('sampledLayers requires exactly three layers')
    decoded = [_layer(layer, i) for i, layer in enumerate(layers)]

    def saturate(value: float) -> float:
        return max(0., min(1., value))

    fades = []
    for scale, horizontal, vertical, width in zip(scales, (29.,116.,464.),
                                                 (13.,52.,208.), (2.,8.,32.)):
        center = [_number(o - c * scale, 'Computed clipmap center')
                  for o, c in zip(origin, camera)]
        distance = [_number(abs(x - c), 'Computed clipmap distance')
                    for x, c in zip(world, center)]
        fades.append(max(saturate((max(distance[0], distance[2])-horizontal)/width),
                         saturate((distance[1]-vertical)/width)))

    active = row['enabled'] and fades[2] < 1.
    weights = [0., 0., 0.]
    u, v = 0., 1.
    coefficients = [color.copy() for color in defaults]
    if active:
        f0, f1, f2 = fades
        weights = [1-f0, f0*(1-f1), f1*(1-f2)]
        alpha = f2 + sum(weight * direction[0][3]
                         for weight, (_, direction) in zip(weights, decoded))
        remapped = saturate(2*alpha-1)
        u, v = remapped-f2, .5*(remapped+f2)
        coefficients = []
        for c, default in enumerate(defaults):
            linear = [sum(weight * amplitude[c] * (4*direction[c][axis]-2)
                          for weight, (amplitude, direction) in zip(weights, decoded))
                      for axis in range(3)]
            dc = sum(weight * amplitude[c]
                     for weight, (amplitude, _) in zip(weights, decoded))
            extra = [v*default[0], v*default[1]+.5*u*default[3],
                     v*default[2], v*default[3]+.375*u*default[1]]
            coefficients.append([_number(a+b, 'Computed irradiance coefficient')
                                 for a, b in zip(linear+[dc], extra)])
    return dict(version=1, coefficients=coefficients, layerWeights=weights,
                clipmapFades=fades, volumeActive=active, defaultMix=dict(u=u, v=v),
                directionalAlpha=[[color[3] for color in direction]
                                  for _, direction in decoded])
