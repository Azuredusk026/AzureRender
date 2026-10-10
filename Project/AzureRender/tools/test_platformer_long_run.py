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


# Recorded public game response at the completed terminal platform.
import json
from pathlib import Path
from run_platformer_long_run import PlatformerScenario
root=Path(__file__).resolve().parents[1]
observed=json.loads((root/'tests/fixtures/platformer-terminal-stop.json').read_text(encoding='utf-8'))
platforms=json.loads((root/'assets_public/platformer/PLATFORMER-DESIGN.json').read_text(encoding='utf-8'))['platforms']
class TerminalStopHarness(PlatformerScenario):
 def __init__(self,state):
  super().__init__();self.state=state;self.address={};self.phase='jump';self.phaseStarted=361.68;self.index=6;self.platforms=platforms;self.keys={87};self.events=[]
 def call(self,op,**fields):
  assert op=='query' and fields['name']=='game.state'
  return json.dumps(self.state)
 def input(self,**event):self.events.append(event)
 def event(self,kind,**fields):pass
class CompletionBoundary(unittest.TestCase):
 def state(self):return copy.deepcopy(observed['value'])
 def test_observed_terminal_stop_still_requires_final_landing(self):
  scenario=TerminalStopHarness(self.state());scenario._update(363.24)
  self.assertEqual(scenario.phase,'land');self.assertEqual(scenario.counts['mandatoryLandings'],0)
  scenario._update(363.4);self.assertEqual(scenario.index,7);self.assertEqual(scenario.counts['mandatoryLandings'],1);self.assertEqual(scenario.phase,'finish')
  scenario._update(363.6);self.assertEqual(scenario.counts['completedRoutes'],1)
 def test_airborne_completion_cannot_count_as_landing(self):
  state=self.state();state['characters'][0]['grounded']=False;scenario=TerminalStopHarness(state)
  scenario._update(363.24);scenario._update(363.4)
  self.assertEqual(scenario.counts['mandatoryLandings'],0);self.assertEqual(scenario.counts['completedRoutes'],0)
 def test_completion_outside_existing_landing_tolerance_is_rejected(self):
  state=self.state();state['characters'][0]['position'][2]=platforms[7]['z']+1.;scenario=TerminalStopHarness(state)
  scenario._update(363.24);scenario._update(363.4)
  self.assertEqual(scenario.counts['mandatoryLandings'],0);self.assertEqual(scenario.counts['completedRoutes'],0)
 def test_nonterminal_jump_keeps_position_condition(self):
  state=self.state();state['tasks'][0]['completed']=False;scenario=TerminalStopHarness(state);scenario._update(363.24)
  self.assertEqual(scenario.phase,'jump');self.assertEqual(scenario.counts['mandatoryLandings'],0)


observedScan=json.loads((root/'tests/fixtures/platformer-background-scan.json').read_text(encoding='utf-8'))
class BackgroundScanHarness(PlatformerScenario):
 def __init__(self):
  super().__init__();self.address={};self.phase='jump';self.phaseStarted=1613.8;self.index=0;self.platforms=platforms;self.keys={87};self.events=[];self.state=copy.deepcopy(observedScan['value'])
 def call(self,op,**fields):
  assert op=='query' and fields['name']=='game.state';return json.dumps(self.state)
 def input(self,**event):self.events.append(event)
 def event(self,*args,**fields):pass
class BackgroundScanBoundary(unittest.TestCase):
 def test_active_world_stops_at_target_during_background_scan(self):
  h=BackgroundScanHarness();h._update(observedScan['seconds']);self.assertEqual(h.phase,'land');self.assertNotIn(87,h.keys)
  self.assertEqual(h.counts['mandatoryLandings'],0)
 def test_background_scan_preserves_existing_grounded_landing_rule(self):
  h=BackgroundScanHarness();h.phase='land';h._update(observedScan['seconds']);self.assertEqual(h.index,1);self.assertEqual(h.counts['mandatoryLandings'],1)
 def test_actual_preload_still_waits_before_scene_switch(self):
  h=BackgroundScanHarness();h.phase='preloading';h.desired=h.exploration;h._update(observedScan['seconds']);self.assertEqual(h.phase,'preloading');self.assertFalse(h.events)
 def test_actual_loading_still_waits_before_route_start(self):
  h=BackgroundScanHarness();h.phase='loading';h.desired=h.course;h.oldRevision=h.state['levelRevision']-1;h.nextPhase='start';h._update(observedScan['seconds']);self.assertEqual(h.phase,'loading');self.assertFalse(h.events)
 def test_airborne_background_scan_cannot_count_a_landing(self):
  h=BackgroundScanHarness();h.phase='land';h.state['characters'][0]['grounded']=False;h._update(observedScan['seconds']);self.assertEqual(h.counts['mandatoryLandings'],0)
 def test_outside_existing_landing_range_stays_rejected(self):
  h=BackgroundScanHarness();h.phase='land';h.state['characters'][0]['position'][2]=platforms[1]['z']-0.817775;h._update(observedScan['seconds']);self.assertEqual(h.counts['mandatoryLandings'],0)
 def test_route_with_unexpected_active_scene_is_rejected(self):
  h=BackgroundScanHarness();h.state['levelReference']=h.exploration
  with self.assertRaises(AssertionError):h._update(observedScan['seconds'])

if __name__=='__main__':unittest.main()
