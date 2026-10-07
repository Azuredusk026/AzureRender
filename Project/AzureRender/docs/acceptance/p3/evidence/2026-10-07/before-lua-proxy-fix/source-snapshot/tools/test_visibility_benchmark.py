import copy
import unittest
from run_visibility_scale_benchmark import validate_scale_report

class ScaleGateTests(unittest.TestCase):
    def test_complete_evidence_and_budget(self):
        report={'runs':[dict(instances=n,variant=v,pixelHashes=['abc','def'],gpuP95Ms=1,cpuWorkP95Ms=2,
            released=True,validationErrors=0,samples=120,warmup=30)
            for n in [100,500,10000] for v in ['cpu','default','prototype','prototype-fixed']]}
        validate_scale_report(report)
        for mutation in ['missing','pixels','budget','release','samples']:
            broken=copy.deepcopy(report)
            if mutation=='missing':broken['runs'].pop()
            if mutation=='pixels':broken['runs'][1]['pixelHashes']=['wrong']
            if mutation=='budget':broken['runs'][-1]['gpuP95Ms']=101
            if mutation=='release':broken['runs'][0]['released']=False
            if mutation=='samples':broken['runs'][0]['samples']=119
            with self.assertRaises(ValueError,msg=mutation):validate_scale_report(broken)

if __name__=='__main__':unittest.main()
