"""Device conditions use driver limits and raw, bounded observations."""
import importlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import threading


class GpuEnvironmentTests(unittest.TestCase):
    def module(self):
        self.assertIsNotNone(importlib.util.find_spec('gpu_environment'),
            'The shared device-environment implementation is required')
        return importlib.import_module('gpu_environment')

    def device(self,**overrides):
        result=dict(temperature=65,thermalLimit=87,gpuUtilization=0,
            thermalSlowdown={'hardware':False,'software':False})
        result.update(overrides);return result

    def window(self):
        return [dict(self.device(),seconds=s) for s in (0,5,10)]

    def test_stable_warm_computer_is_ready(self):
        self.assertEqual(self.module().startup_errors(self.window()),[])

    def test_device_limit_is_respected(self):
        for temperature in (86,87,90):
            samples=[dict(self.device(temperature=temperature),seconds=s) for s in (0,5,10)]
            self.assertTrue(self.module().startup_errors(samples))

    def test_background_load_and_unstable_temperature_wait(self):
        samples=self.window();samples[-1]['gpuUtilization']=30
        self.assertTrue(self.module().startup_errors(samples))
        samples=self.window();samples[0]['temperature']=58
        self.assertTrue(self.module().startup_errors(samples))

    def test_desktop_load_limit_is_an_explicit_policy(self):
        samples=self.window()
        for row in samples:row['gpuUtilization']=20
        self.assertEqual(self.module().startup_errors(samples,{'maxIdleUtilization':25}),[])

    def test_missing_and_invalid_telemetry_is_diagnosed(self):
        for value in (None,float('nan'),-1,True):
            self.assertTrue(self.module().device_errors(self.device(temperature=value)))
        self.assertTrue(self.module().device_errors(self.device(thermalSlowdown={})))
        self.assertEqual(self.module().device_errors(self.device(thermalLimit=None)),[])

    def test_short_or_nonmonotonic_window_is_rejected(self):
        self.assertTrue(self.module().startup_errors(self.window()[:2]))
        samples=self.window();samples[-1]['seconds']=4
        self.assertTrue(self.module().startup_errors(samples))

    def test_modern_driver_temperature_margin_is_decoded(self):
        xml='''<nvidia_smi_log><driver_version>1</driver_version><gpu id="0"><product_name>GPU</product_name>
        <temperature><gpu_temp>65 C</gpu_temp><gpu_temp_tlimit>22 C</gpu_temp_tlimit>
        <gpu_temp_slow_tlimit_threshold>-2 C</gpu_temp_slow_tlimit_threshold>
        <gpu_temp_max_gpu_tlimit_threshold>0 C</gpu_temp_max_gpu_tlimit_threshold></temperature>
        <utilization><gpu_util>3 %</gpu_util></utilization><clocks_event_reasons>
        <clocks_event_reason_hw_thermal_slowdown>Not Active</clocks_event_reason_hw_thermal_slowdown>
        <clocks_event_reason_sw_thermal_slowdown>Not Active</clocks_event_reason_sw_thermal_slowdown>
        <clocks_event_reason_sw_power_cap>Active</clocks_event_reason_sw_power_cap>
        </clocks_event_reasons></gpu></nvidia_smi_log>'''
        row=self.module().parse_snapshot(xml)
        self.assertIsNone(row['thermalLimit'])
        self.assertEqual(row['thermalMargin'],22)
        self.assertEqual(row['relativeThermalThreshold'],0)
        self.assertEqual(row['gpuUtilization'],3)
        self.assertEqual(self.module().device_errors(row),[])
        self.assertEqual(row['powerCap'],'Active')

    def test_relative_margin_is_checked_without_inventing_absolute_limits(self):
        module=self.module()
        xml='''<nvidia_smi_log><gpu><temperature><gpu_temp>88 C</gpu_temp>
        <gpu_temp_tlimit>15 C</gpu_temp_tlimit><gpu_temp_slow_tlimit_threshold>-2 C</gpu_temp_slow_tlimit_threshold>
        <gpu_temp_max_gpu_tlimit_threshold>0 C</gpu_temp_max_gpu_tlimit_threshold>
        <gpu_target_temperature>87 C</gpu_target_temperature></temperature>
        <utilization><gpu_util>0 %</gpu_util></utilization><clocks_event_reasons>
        <clocks_event_reason_hw_thermal_slowdown>Not Active</clocks_event_reason_hw_thermal_slowdown>
        <clocks_event_reason_sw_thermal_slowdown>Not Active</clocks_event_reason_sw_thermal_slowdown>
        </clocks_event_reasons></gpu></nvidia_smi_log>'''
        device=module.parse_snapshot(xml)
        self.assertIsNone(device['thermalLimit'])
        self.assertEqual(device['targetTemperature'],87)
        self.assertEqual(module.device_errors(device,startup=True),[])
        for margin in (0,-1):
            self.assertTrue(module.device_errors(dict(device,thermalMargin=margin)))
        self.assertTrue(module.device_errors(dict(device,thermalMargin=1),startup=True))
        self.assertEqual(module.device_errors(dict(device,thermalMargin=1)),[])

    def test_relative_threshold_requires_valid_relative_telemetry(self):
        module=self.module()
        for margin in (None,float('nan'),True):
            device=self.device(thermalLimit=None,relativeThermalThreshold=0,thermalMargin=margin)
            self.assertTrue(module.device_errors(device))

    def test_legacy_driver_limits_and_thermal_flags_are_decoded(self):
        xml='''<nvidia_smi_log><driver_version>1</driver_version><gpu><product_name>GPU</product_name>
        <temperature><gpu_temp>70 C</gpu_temp><gpu_temp_slow_threshold>93 C</gpu_temp_slow_threshold>
        <gpu_temp_max_gpu_threshold>90 C</gpu_temp_max_gpu_threshold></temperature>
        <utilization><gpu_util>0 %</gpu_util></utilization><clocks_throttle_reasons>
        <clocks_throttle_reason_hw_thermal_slowdown>Active</clocks_throttle_reason_hw_thermal_slowdown>
        <clocks_throttle_reason_sw_thermal_slowdown>Not Active</clocks_throttle_reason_sw_thermal_slowdown>
        </clocks_throttle_reasons></gpu></nvidia_smi_log>'''
        row=self.module().parse_snapshot(xml)
        self.assertEqual(row['thermalLimit'],90)
        self.assertTrue(self.module().device_errors(row))

    def test_wait_records_every_sample_and_has_a_deadline(self):
        module=self.module();clock=[0]
        def sleep(seconds):clock[0]+=seconds
        samples=[]
        result=module.wait_until_ready(read=self.device,clock=lambda:clock[0],sleep=sleep,
            observations=samples)
        self.assertEqual(len(samples),3);self.assertEqual(result['startup'],samples)
        samples=[];clock[0]=0
        with self.assertRaisesRegex(TimeoutError,'not ready'):
            module.wait_until_ready(read=lambda:self.device(gpuUtilization=50),
                clock=lambda:clock[0],sleep=sleep,timeout=15,observations=samples)
        self.assertEqual(len(samples),4)

    def test_timeout_keeps_the_raw_observations(self):
        module=self.module();clock=[0]
        def sleep(seconds):clock[0]+=seconds
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'environment.json'
            with self.assertRaises(TimeoutError):
                module.wait_and_record(path,read=lambda:self.device(gpuUtilization=50),
                    clock=lambda:clock[0],sleep=sleep,timeout=15)
            data=json.loads(path.read_text())
        self.assertEqual(data['errorType'],'TimeoutError')
        self.assertEqual(len(data['observations']),4)
        self.assertTrue(module.environment_errors(data))

    def continuous(self):
        return dict(schemaVersion=2, policy={}, startup=self.window(), final=self.device(),
            coverage='PeriodicDuringRun', runSeconds=20,
            runObservations=[dict(self.device(),seconds=s) for s in (0,5,10,15,20)],
            monitorErrors=[])

    def test_midrun_slowdown_is_rejected_even_when_endpoints_are_ready(self):
        data=self.continuous()
        self.assertEqual(self.module().environment_errors(data),[])
        data['runObservations'][2]['thermalSlowdown']['software']=True
        errors=self.module().environment_errors(data)
        self.assertTrue(any('software thermal slowdown is active' in error for error in errors),errors)

    def test_missing_midrun_samples_or_monitor_failure_is_rejected(self):
        for field,value in [('runObservations',[]),('runSeconds',float('nan')),
                            ('monitorErrors',['driver query failed']),('coverage','StartupWindowAndEndObservation')]:
            data=self.continuous();data[field]=value
            self.assertTrue(self.module().environment_errors(data),field)
        data=self.continuous();data['runObservations']=[data['runObservations'][0]]
        self.assertTrue(self.module().environment_errors(data))
        data=self.continuous();data['runObservations'][2]['seconds']=4
        self.assertTrue(self.module().environment_errors(data))

    def test_long_unobserved_interval_is_rejected(self):
        data=self.continuous();data['runSeconds']=60
        data['runObservations']=[dict(self.device(),seconds=s) for s in (0,5,60)]
        self.assertTrue(self.module().environment_errors(data))

    def test_driver_power_and_clock_are_retained_for_correlation(self):
        xml='''<nvidia_smi_log><gpu><power_readings><power_draw>42.50 W</power_draw>
        <power_limit>100.00 W</power_limit></power_readings><clocks><graphics_clock>2100 MHz</graphics_clock>
        <sm_clock>2100 MHz</sm_clock><mem_clock>8001 MHz</mem_clock></clocks></gpu></nvidia_smi_log>'''
        device=self.module().parse_snapshot(xml)
        self.assertEqual(device.get('powerDrawWatts'),42.5)
        self.assertEqual(device.get('graphicsClockMHz'),2100)
        self.assertEqual(device.get('memoryClockMHz'),8001)

    def test_modern_driver_instant_power_and_enforced_limit_are_retained(self):
        xml='''<nvidia_smi_log><gpu><gpu_power_readings><average_power_draw>79.88 W</average_power_draw>
        <instant_power_draw>79.51 W</instant_power_draw><current_power_limit>80.00 W</current_power_limit>
        </gpu_power_readings></gpu></nvidia_smi_log>'''
        device=self.module().parse_snapshot(xml)
        self.assertEqual(device.get('powerDrawWatts'),79.51)
        self.assertEqual(device.get('powerLimitWatts'),80)

    def test_monitor_owns_thread_and_preserves_raw_observations(self):
        module=self.module();self.assertTrue(hasattr(module,'RunEnvironmentMonitor'))
        observed=threading.Event()
        def read():
            observed.set();return self.device()
        evidence=dict(schemaVersion=1,startup=self.window(),policy={})
        monitor=module.RunEnvironmentMonitor(evidence,read=read,interval=.01)
        with monitor:
            self.assertTrue(observed.wait(timeout=2))
        monitor.close()
        self.assertFalse(monitor.worker.is_alive())
        self.assertGreaterEqual(len(evidence['runObservations']),2)
        self.assertEqual(evidence['runObservations'][0]['seconds'],0)
        self.assertEqual(module.environment_errors(evidence),[])

    def test_monitor_query_failure_is_saved_and_fails_acceptance(self):
        module=self.module();self.assertTrue(hasattr(module,'RunEnvironmentMonitor'))
        evidence=dict(schemaVersion=1,startup=self.window(),policy={})
        def read():raise RuntimeError('driver unavailable')
        with module.RunEnvironmentMonitor(evidence,read=read,interval=.01):pass
        self.assertTrue(any('driver unavailable' in error for error in evidence['monitorErrors']))
        self.assertTrue(module.environment_errors(evidence))

    def test_failed_workload_still_writes_the_environment_evidence(self):
        module=self.module();evidence=dict(schemaVersion=1,startup=self.window(),policy={})
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'environment.json'
            with self.assertRaisesRegex(RuntimeError,'workload failed'):
                with module.RunEnvironmentMonitor(evidence,path=path,read=self.device,interval=.01):
                    raise RuntimeError('workload failed')
            data=json.loads(path.read_text(encoding='utf-8'))
            self.assertEqual(data['schemaVersion'],2)
            self.assertGreaterEqual(len(data['runObservations']),2)


if __name__=='__main__':unittest.main()
