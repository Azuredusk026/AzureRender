"""Observe real driver thermal limits and wait for a stable idle window."""
import math
import json
import re
import subprocess
import time
import xml.etree.ElementTree as ET

DEFAULT_POLICY=dict(idleSeconds=10,temperatureRange=4,maxIdleUtilization=10,startHeadroom=2)


def number(text):
    match=re.fullmatch(r'\s*(-?\d+(?:\.\d+)?)\s*(?:C|%)?\s*',text or '')
    return float(match[1]) if match else None


def parse_snapshot(xml):
    root=ET.fromstring(xml)
    gpu=root.find('gpu')
    if gpu is None:raise ValueError('NVIDIA driver did not report a GPU')
    temperature=number(gpu.findtext('temperature/gpu_temp'))
    limits=[]
    for field in ('gpu_temp_slow_threshold','gpu_temp_max_gpu_threshold'):
        value=number(gpu.findtext('temperature/'+field))
        if value is not None:limits.append(value)
    margin=number(gpu.findtext('temperature/gpu_temp_tlimit'))
    relative=[]
    for field in ('gpu_temp_slow_tlimit_threshold','gpu_temp_max_gpu_tlimit_threshold'):
        value=number(gpu.findtext('temperature/'+field))
        if value is not None:relative.append(value)
    reasons=gpu.find('clocks_event_reasons')
    prefix='clocks_event_reason_'
    if reasons is None:
        reasons=gpu.find('clocks_throttle_reasons');prefix='clocks_throttle_reason_'
    def reason(name):return reasons.findtext(prefix+name) if reasons is not None else None
    def active(name):return {'Active':True,'Not Active':False}.get(reason(name))
    return dict(temperature=temperature,thermalLimit=min(limits) if limits else None,
        thermalLimits=limits,thermalMargin=margin,relativeThermalThreshold=max(relative) if relative else None,
        relativeThermalThresholds=relative,targetTemperature=number(gpu.findtext('temperature/gpu_target_temperature')),
        thermalLimitStatus='Reported' if limits else 'RelativeReported' if relative else 'Unavailable',
        thermalSlowdown=dict(hardware=active('hw_thermal_slowdown'),software=active('sw_thermal_slowdown')),
        powerCap=reason('sw_power_cap'),gpuUtilization=number(gpu.findtext('utilization/gpu_util')),
        gpu=gpu.findtext('product_name'),driver=root.findtext('driver_version'),
        deviceId=gpu.get('id'),rawXml=xml)


def snapshot():
    result=subprocess.run(['nvidia-smi','-q','-x'],capture_output=True,text=True,check=True,timeout=15)
    return parse_snapshot(result.stdout)


def valid_number(value):
    return not isinstance(value,bool) and isinstance(value,(int,float)) and math.isfinite(value)


def policy_values(policy=None):
    result=dict(DEFAULT_POLICY)
    if policy is not None:
        if not isinstance(policy,dict) or set(policy)-set(result):raise ValueError('Invalid device-environment policy')
        result.update(policy)
    for key,value in result.items():
        if not valid_number(value) or value<0:raise ValueError('Invalid device-environment policy: '+key)
    if result['idleSeconds']<10 or result['maxIdleUtilization']>100:
        raise ValueError('Invalid idle window or GPU utilization limit')
    return result


def device_errors(device,*,startup=False,policy=None):
    policy=policy_values(policy)
    if not isinstance(device,dict):return ['device observation is missing']
    errors=[]
    temperature=device.get('temperature')
    if not valid_number(temperature) or temperature<=0:errors.append('temperature is missing or invalid')
    utilization=device.get('gpuUtilization')
    if not valid_number(utilization) or not 0<=utilization<=100:
        errors.append('GPU utilization is missing or invalid')
    elif startup and utilization>policy['maxIdleUtilization']:
        errors.append('background GPU load exceeds the declared idle limit')
    limit=device.get('thermalLimit')
    if limit is not None:
        if not valid_number(limit) or limit<=0:errors.append('driver thermal limit is invalid')
        elif valid_number(temperature):
            margin=policy['startHeadroom'] if startup else 0
            if temperature>=limit or temperature>limit-margin:
                errors.append('driver thermal headroom is insufficient')
    relative=device.get('relativeThermalThreshold')
    if relative is not None:
        margin=device.get('thermalMargin')
        if not valid_number(relative):errors.append('driver relative thermal threshold is invalid')
        elif not valid_number(margin):errors.append('driver relative thermal margin is missing or invalid')
        elif margin<=relative or (startup and margin-relative<policy['startHeadroom']):
            errors.append('driver relative thermal headroom is insufficient')
    reasons=device.get('thermalSlowdown',{})
    for key in ('hardware','software'):
        state=reasons.get(key) if isinstance(reasons,dict) else None
        if not isinstance(state,bool):errors.append(key+' thermal slowdown telemetry is missing')
        elif state:errors.append(key+' thermal slowdown is active')
    return errors


def startup_errors(samples,policy=None):
    policy=policy_values(policy)
    if not isinstance(samples,list) or len(samples)<3:return ['at least three startup observations are required']
    errors=[]
    for row in samples:errors.extend(device_errors(row,startup=True,policy=policy))
    times=[row.get('seconds') if isinstance(row,dict) else None for row in samples]
    if (any(not valid_number(t) or t<0 for t in times)
            or any(b<=a for a,b in zip(times,times[1:]))):
        errors.append('startup observation times are invalid')
    elif times[-1]-times[0]<policy['idleSeconds']:
        errors.append('startup observation window is too short')
    temperatures=[row.get('temperature') if isinstance(row,dict) else None for row in samples]
    if all(valid_number(t) for t in temperatures) and max(temperatures)-min(temperatures)>policy['temperatureRange']:
        errors.append('idle temperature has not stabilized')
    return errors


def environment_errors(evidence):
    if not isinstance(evidence,dict) or evidence.get('schemaVersion')!=1:
        return ['device environment evidence is missing or unsupported']
    try:
        policy=policy_values(evidence.get('policy'))
        return startup_errors(evidence.get('startup'),policy)+device_errors(evidence.get('final'),policy=policy)
    except (TypeError,ValueError) as error:return [str(error)]


def wait_until_ready(*,read=snapshot,clock=time.monotonic,sleep=time.sleep,timeout=180,
                     policy=None,observations=None):
    policy=policy_values(policy)
    if not valid_number(timeout) or timeout<policy['idleSeconds']:raise ValueError('Invalid environment wait deadline')
    observations=[] if observations is None else observations
    start=clock()
    while True:
        device=read();row=dict(device,seconds=clock()-start);observations.append(row)
        window=observations[-3:]
        errors=startup_errors(window,policy)
        if not errors:
            return dict(schemaVersion=1,policy=policy,startup=window,observations=list(observations),
                coverage='StartupWindowAndEndObservation')
        if clock()-start>=timeout:raise TimeoutError('GPU environment not ready: '+'; '.join(errors))
        sleep(5)


def wait_and_record(path,*,policy=None,**options):
    observations=[]
    result=dict(schemaVersion=1,policy=policy_values(policy),observations=observations,startup=[],
        coverage='StartupWindowAndEndObservation')
    try:
        result=wait_until_ready(policy=policy,observations=observations,**options)
        return result
    except Exception as error:
        result.update(errorType=type(error).__name__,error=str(error))
        raise
    finally:path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
