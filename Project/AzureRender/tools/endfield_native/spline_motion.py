"""Scalar reference for statically observed spline timing and distance sampling.

This importer-side evaluator uses Python precision, not bit-identical native
floating point. It does not resolve live activation, hot patches or random seeds.
"""
import math


def _number(value, name, positive=False):
    if type(value) not in (int, float) or not math.isfinite(value) or value < 0 or (positive and value == 0):
        raise ValueError(f'invalid {name}')
    return value


def advance_emission(remainder, delta_time, simulation_speed, emission_rate):
    """Return birth ages in simulation time, oldest first, and unused time."""
    for name, value in (('remainder', remainder), ('delta time', delta_time),
                        ('simulation speed', simulation_speed), ('emission rate', emission_rate)):
        _number(value, name)
    if simulation_speed == 0:
        return [], remainder
    elapsed = remainder + delta_time * simulation_speed
    _number(elapsed, 'elapsed time')
    if emission_rate == 0:
        return [], elapsed
    count_value = elapsed * emission_rate
    if not math.isfinite(count_value) or count_value > 1000000:
        raise ValueError('emission batch exceeds importer budget')
    count = int(count_value)
    interval = 1 / emission_rate
    return ([elapsed - (index + 1) * interval for index in range(count)],
            elapsed - count * interval)


def sample_distance(segments, distance):
    """Evaluate cubic controls at distance / stored segment length.

    Every supplied slot participates. Equality advances to the next slot;
    reaching the final length returns None, matching the native expiry branch.
    """
    _number(distance, 'distance')
    if not segments:
        raise ValueError('empty spline path')
    for segment in segments:
        _number(segment['stored_length'], 'stored segment length', positive=True)
        points = segment['controls']
        if len(points) != 4 or any(len(point) != 3 for point in points):
            raise ValueError('cubic path requires four three-component controls')
        if any(type(v) not in (int, float) or not math.isfinite(v) for point in points for v in point):
            raise ValueError('non-finite path control')
    previous = 0.0
    for segment in segments:
        length = segment['stored_length']
        if previous + length > distance:
            t = (distance - previous) / length
            weights = ((1-t)**3, 3*(1-t)**2*t, 3*(1-t)*t*t, t**3)
            return [sum(weight * point[axis] for weight, point in zip(weights, segment['controls']))
                    for axis in range(3)]
        previous += length
    return None


def constant_timing(total_length, velocity, lifetime_fraction, emission_rate, simulation_speed):
    """Describe the source constant-velocity, constant-lifetime branch only."""
    for name, value in (('path length', total_length), ('velocity', velocity),
                        ('emission rate', emission_rate), ('simulation speed', simulation_speed)):
        _number(value, name, positive=True)
    if type(lifetime_fraction) not in (int, float) or not math.isfinite(lifetime_fraction):
        raise ValueError('invalid lifetime fraction')
    lifetime = max(0, min(1, lifetime_fraction)) * total_length / velocity
    prewarm = lifetime * emission_rate
    if not math.isfinite(prewarm) or prewarm > 1000000:
        raise ValueError('prewarm batch exceeds importer budget')
    return {'simulation_lifetime': lifetime, 'wall_lifetime': lifetime / simulation_speed,
            'wall_emission_interval': 1 / (emission_rate * simulation_speed),
            'prewarm_count': math.ceil(prewarm)}
