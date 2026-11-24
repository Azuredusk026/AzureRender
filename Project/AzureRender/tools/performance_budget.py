"""Strict P5 budgets; callers retain raw observations and every failure."""
import math
from gpu_environment import device_errors,environment_errors

BUDGETS = dict(gpuP95Ms=16.6, playerCpuP95Ms=8, editorCpuP95Ms=10,
    physicsP95Ms=2, stableWorkingSetGrowth=.05,
    peakWorkingSetBytes=2*1024**3, deviceLocalBytes=1.5*1024**3)


def working_set_growth(samples: list[int]) -> float:
    if len(samples)<10 or any(isinstance(v,bool) or not isinstance(v,(int,float))
            or not math.isfinite(v) or v<=0 for v in samples):
        raise ValueError('Insufficient valid working-set observations')
    stable=samples[len(samples)//2:]
    return max(stable)/stable[0]-1


def validate_round(result: dict, *, editor: bool) -> list[str]:
    errors=[]
    def bounded(name, value, limit, *, positive=False):
        if isinstance(value,bool) or not isinstance(value,(int,float)) or not math.isfinite(value):
            errors.append(f'{name}: missing or non-finite value')
        elif value<0 or (positive and value==0):
            errors.append(f'{name}: invalid value {value}')
        elif value>limit and not math.isclose(value,limit,rel_tol=0,abs_tol=1e-12):
            errors.append(f'{name}: {value} exceeds {limit}')
    for field,limit in [('gpu',16.6),('cpu',10 if editor else 8),('physics',2)]:
        metric=result.get(field)
        bounded(field+'.p95',metric.get('p95') if isinstance(metric,dict) else None,limit)
    for key,expected in [('warmup',300),('samples',1800)]:
        if result.get(key)!=expected: errors.append(f'{key}: expected {expected}')
    bounded('peakWorkingSetBytes',result.get('peakWorkingSetBytes'),2*1024**3,positive=True)
    allocator=result.get('allocator',{})
    bounded('deviceLocalPeakBytes',allocator.get('deviceLocalPeakBytes') if isinstance(allocator,dict) else None,1.5*1024**3)
    initial=result.get('initialDevice',{})
    errors.extend('initial environment: '+error for error in device_errors(initial))
    errors.extend('device environment: '+error for error in environment_errors(result.get('environment')))
    environment=result.get('environment')
    if isinstance(environment,dict) and isinstance(environment.get('startup'),list) and environment['startup']:
        recorded=environment['startup'][-1]
        if isinstance(initial,dict) and isinstance(recorded,dict):
            for field in ('temperature','thermalLimit','thermalMargin','relativeThermalThreshold',
                          'targetTemperature','gpuUtilization','thermalSlowdown'):
                if initial.get(field)!=recorded.get(field):errors.append('initial environment differs from raw startup: '+field)
    try:
        growth=working_set_growth(result.get('workingSetSamplesBytes',[]))
        bounded('stableWorkingSetGrowth',growth,.05)
        claimed=result.get('stableWorkingSetGrowth')
        if not isinstance(claimed,(int,float)) or not math.isfinite(claimed) or not math.isclose(claimed,growth,rel_tol=0,abs_tol=1e-12):
            errors.append('stableWorkingSetGrowth: does not match raw observations')
    except (TypeError,ValueError) as error:
        errors.append('workingSetSamplesBytes: '+str(error))
    return errors
