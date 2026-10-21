"""Compile source constant spline emitters into independent rigid-node clips.

The authored phase and optional random slot samples are candidate choices.
Native activation, global phase and random-generator agreement need runtime
evidence. Outputs use source-local coordinates and raw source Euler values.
"""
import hashlib
import math
import struct

from endfield_native.spline_motion import constant_timing, periodic_prewarm, sample_distance


def _finite(value, name, positive=False):
    if type(value) not in (int, float) or not math.isfinite(value) or (positive and value <= 0):
        raise ValueError('invalid ' + name)
    return value


def compile_emitter(emitter, sample_rate=30, seed=None, max_keys=1000000):
    """Create per-slot translation LINEAR and visibility/scale STEP key data.

    An explicit seed permits stable per-slot samples of TwoConstants ranges.
    This does not emulate resampling at every birth or the game's random seed.
    Unsupported curve/noise/continuous rotation modes fail before producing data.
    """
    _finite(sample_rate, 'sample rate', positive=True)
    if sample_rate > 1000 or type(max_keys) is not int or max_keys < 1:
        raise ValueError('invalid key budget or sample rate')
    if seed is not None and type(seed) is not int:
        raise ValueError('seed must be an integer')
    identity = emitter['identity']
    if (not isinstance(identity, (list, tuple)) or len(identity) != 2
            or not isinstance(identity[0], str) or type(identity[1]) is not int):
        raise ValueError('invalid emitter identity')
    fields = emitter['fields']

    def value(name):
        try:
            return fields['desc/' + name]['value']
        except KeyError as error:
            raise ValueError('missing source field ' + name) from error

    for name in ('hasNoise', 'hasRotation', 'has3dRotation'):
        if value(name) != 0:
            raise ValueError('unsupported ' + name)
    for name, supported in (('prewarm', 1), ('emissionType', 0), ('has3dStartRotation', 1)):
        if value(name) != supported:
            raise ValueError('unsupported ' + name)

    def curve(name, slot=0, allow_random=False):
        mode = value(name + '/m_Mode')
        maximum = _finite(value(name + '/m_ConstantMax'), name)
        if mode == 0:
            return maximum
        if mode == 3:
            minimum = _finite(value(name + '/m_ConstantMin'), name)
            if minimum == maximum:
                return maximum
            if not allow_random:
                raise ValueError('unsupported source range ' + name)
            if seed is None:
                raise ValueError('explicit seed required for ' + name)
            key = f'{identity[0]}:{identity[1]}:{seed}:{slot}:{name}'.encode()
            fraction = int.from_bytes(hashlib.sha256(key).digest()[:8], 'little') / 2**64
            return minimum + fraction * (maximum - minimum)
        raise ValueError('unsupported source curve ' + name)

    rate = _finite(value('emissionRateOverTime'), 'emission rate', positive=True)
    speed = _finite(value('simulateSpeed'), 'simulation speed', positive=True)
    segments = emitter['segments']
    # Validate every segment, including the terminal source cache slot.
    sample_distance(segments, 0)
    timing = constant_timing(sum(s['stored_length'] for s in segments),
                             curve('velocityOverLifeTime'), curve('startLifeTime'), rate, speed)
    life = timing['simulation_lifetime']
    pool = periodic_prewarm(life, rate, speed, 0)
    period, count = pool['period'], len(pool['ages'])
    if not count:
        raise ValueError('empty source lifetime')
    sample_count = _finite(period * sample_rate, 'key budget', positive=True)
    estimated_keys = count * (math.ceil(sample_count) + 6)
    if estimated_keys > max_keys:
        raise ValueError('spline key budget exceeded')
    velocity = curve('velocityOverLifeTime')
    alpha_keys = []
    alpha_constant = 1.
    if value('hasAlphaControl'):
        mode = value('alphaOverLifeTime/m_Mode')
        if mode == 0:
            alpha_constant = curve('alphaOverLifeTime')
        elif mode == 1:
            prefix = 'alphaOverLifeTime/m_CurveMax/m_Curve/size'
            key_count = value(prefix)
            if type(key_count) is not int or not 1 <= key_count <= 1024:
                raise ValueError('invalid alpha curve key count')
            multiplier = _finite(value('alphaOverLifeTime/m_CurveMultiplier'), 'alpha multiplier')
            for index in range(key_count):
                key = {name: _finite(value(f'{prefix}/data[{index}]/' + name), 'alpha ' + name)
                       for name in ('time', 'value', 'inSlope', 'outSlope')}
                if value(f'{prefix}/data[{index}]/weightedMode') != 0:
                    raise ValueError('unsupported weighted alpha curve')
                if not 0 <= key['time'] <= 1 or (alpha_keys and key['time'] <= alpha_keys[-1]['time']):
                    raise ValueError('invalid alpha curve times')
                for name in ('value', 'inSlope', 'outSlope'):
                    key[name] *= multiplier
                alpha_keys.append(key)
        else:
            raise ValueError('unsupported alphaOverLifeTime curve')
    if estimated_keys + count * len(alpha_keys) > max_keys:
        raise ValueError('spline alpha key budget exceeded')

    def opacity(age):
        if age is None:
            return 0.
        fraction = age / life
        result = alpha_constant
        if alpha_keys:
            result = alpha_keys[0]['value'] if fraction <= alpha_keys[0]['time'] else alpha_keys[-1]['value']
            for left, right in zip(alpha_keys, alpha_keys[1:]):
                if left['time'] <= fraction <= right['time']:
                    duration = right['time'] - left['time']
                    t = (fraction - left['time']) / duration
                    result = ((2*t**3-3*t*t+1)*left['value']
                              + (t**3-2*t*t+t)*duration*left['outSlope']
                              + (-2*t**3+3*t*t)*right['value']
                              + (t**3-t*t)*duration*right['inSlope'])
                    break
        return max(0., min(1., result))

    slots = []
    for slot in range(count):
        size = curve('startSize', slot, True) * curve('sizeOverLifeTime')
        if size <= 0:
            raise ValueError('invalid startSize')
        rotation = [curve('startRotation' + axis, slot, True) for axis in 'XYZ']
        def time32(value):
            return struct.unpack('<f', struct.pack('<f', value))[0]

        # Event keys own any float32 bucket also reached by a regular sample.
        times = {time32(i / sample_rate): i / sample_rate
                 for i in range(1, math.ceil(period * sample_rate))
                 if i / sample_rate < period}
        times.update({0.: 0., time32(period): period})
        offset = slot / rate
        for event in ((life - offset) / speed, (count / rate - offset) / speed):
            if 0 < event < period:
                previous = max(0., event - max(.00001, period * 2e-7))
                times.update({time32(previous): previous, time32(event): event})
        for key in alpha_keys:
            event = (key['time'] * life - offset) / speed
            if event < 0:
                event += period
            if 0 < event < period:
                times[time32(event)] = event
        ordered = sorted(times.items())
        positions, scales, opacities = [], [], []
        for _, wall_time in ordered:
            age = periodic_prewarm(life, rate, speed, 0 if wall_time == period else wall_time)['ages'][slot]
            point = sample_distance(segments, velocity * age if age is not None else 0)
            if point is None:
                raise ValueError('live slot sampled outside source path')
            positions.append(point)
            scales.append([size] * 3 if age is not None else [0., 0., 0.])
            opacities.append(opacity(age))
        slots.append({'times': [item[0] for item in ordered], 'positions': positions, 'scales': scales,
                      'sourceEuler': rotation, 'opacities': opacities})
    return {'format': 'NativeSplineRigidClip/1', 'identity': list(identity),
            'name': emitter['name'], 'period': period, 'timing': timing, 'slots': slots,
            'sampleRate': sample_rate, 'seed': seed,
            'nativeRandomVerified': False, 'nativeGlobalPhaseVerified': False,
            'nativeActivationVerified': False}


def compile_emitters(emitters, **options):
    """Keep full source identities and independent periods for each source."""
    identities = [tuple(e['identity']) for e in emitters]
    if len(set(identities)) != len(identities):
        raise ValueError('duplicate emitter identity')
    return [compile_emitter(emitter, **options) for emitter in emitters]
