"""Reject duration, route, checkpoint and moving-support coverage gaps."""
import copy
import unittest
from test_playable_delivery_rules import DeliveryRules
from run_platformer_long_run import evaluate_platformer

class PlatformerLongRunRules(unittest.TestCase):
    def valid(self):
        data=DeliveryRules().valid()
        data.update(completedRoutes=10,mandatoryLandings=70,startFallRecoveries=10,
            checkpointFallRecoveries=10,horizontalRides=10,verticalRides=10,
            courseCoins=8,liveObservation=True,controllerErrors=[])
        return data
    def test_complete_real_routes_pass(self):evaluate_platformer(self.valid())
    def test_incomplete_routes_and_feedback_fail(self):
        for field,value in [('completedRoutes',9),('mandatoryLandings',69),
            ('startFallRecoveries',9),('checkpointFallRecoveries',9),
            ('horizontalRides',9),('verticalRides',9),('courseCoins',7),
            ('liveObservation',False),('controllerErrors',['unreachable landing'])]:
            data=copy.deepcopy(self.valid());data[field]=value
            with self.assertRaises(AssertionError,msg=field):evaluate_platformer(data)
    def test_shared_performance_and_real_duration_remain_required(self):
        for field,value in [('actualSeconds',1799),('realTime',False),
            ('workingSetGrowth',.051),('gpu',dict(p95=16.7)),('restarts',9),('switches',9)]:
            data=copy.deepcopy(self.valid());data[field]=value
            with self.assertRaises(AssertionError,msg=field):evaluate_platformer(data)

if __name__=='__main__':unittest.main()
