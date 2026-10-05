def working_set_growth(samples):
    if len(samples) < 10 or any(value <= 0 for value in samples):
        raise ValueError('Insufficient valid working-set observations')
    stable = samples[len(samples) // 2:]
    return max(stable) / stable[0] - 1
