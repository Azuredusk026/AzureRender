"""Keep the delivery gate sensitive to duration, leaks and budget failures."""
import copy
import unittest
from run_playable_long_run import evaluate, periodic_statistics


class DeliveryRules(unittest.TestCase):
    def valid(self):
        return dict(actualSeconds=1801,realTime=True,frames=108000,exitCode=0,
            validationErrors=0,released=True,scriptErrors=[],presentationErrors=[],loadErrors=[],
            switches=12,restarts=12,windowChecks=dict(minimized=True,restored=True,resized=True,finalExtent=[1920,1080]),
            gpu=dict(p95=4),cpu=dict(p95=3),physics=dict(p95=1),
            periodicCpu=dict(p95=3),periodicPhysics=dict(p95=1),
            commitExtra=dict(p99=4),commitFrame=dict(p99=12),
            gpuWindows=[dict(p95=4)],residencyGrowth=dict(buffers=0,images=0,bufferBytes=0,imageBytes=0),
            workingSetGrowth=.02,peakWorkingSetBytes=300_000_000,deviceLocalPeakBytes=200_000_000)

    def test_valid_real_time_delivery(self):
        evaluate(self.valid())

    def test_duration_cannot_be_replaced_by_simulated_frames(self):
        for field,value in (('actualSeconds',1799),('realTime',False)):
            data=self.valid();data[field]=value
            with self.assertRaises(AssertionError):evaluate(data)

    def test_missing_cycles_or_window_recovery_rejects_delivery(self):
        for field in ('switches','restarts'):
            data=self.valid();data[field]=9
            with self.assertRaises(AssertionError):evaluate(data)
        data=self.valid();data['windowChecks']['restored']=False
        with self.assertRaises(AssertionError):evaluate(data)

    def test_resource_and_working_set_growth_rejects_delivery(self):
        for field in self.valid()['residencyGrowth']:
            data=self.valid();data['residencyGrowth'][field]=.051
            with self.assertRaises(AssertionError):evaluate(data)
        data=self.valid();data['workingSetGrowth']=.051
        with self.assertRaises(AssertionError):evaluate(data)

    def test_whole_run_budget_and_shutdown_errors_reject_delivery(self):
        changes=[('gpu',{'p95':16.7}),('cpu',{'p95':8.1}),('physics',{'p95':2.1}),
            ('periodicCpu',{'p95':8.1}),('gpuWindows',[{'p95':16.7}]),
            ('commitExtra',{'p99':8.1}),('commitFrame',{'p99':33.4}),
            ('validationErrors',1),('released',False),('loadErrors',['failed'])]
        for field,value in changes:
            data=copy.deepcopy(self.valid());data[field]=value
            with self.assertRaises(AssertionError):evaluate(data)

    def test_commit_diagnostics_do_not_change_regular_frame_sampling(self):
        rows=[dict(frame=i*240,workMs=2,waitMs=.1,physicsMaxMs=.5,committed=False,commitMs=0) for i in range(2,102)]
        rows += [dict(frame=i*1200+1,workMs=10,waitMs=.1,physicsMaxMs=.8,committed=True,commitMs=4) for i in range(1,21)]
        result=periodic_statistics(rows,0)
        self.assertEqual(result['periodicCpu']['p95'],2)
        self.assertEqual(result['periodicSampleCount'],100)
        self.assertEqual(result['commitExtra']['p99'],4)
        self.assertAlmostEqual(result['commitFrame']['p99'],10.1)


if __name__=='__main__':unittest.main()
