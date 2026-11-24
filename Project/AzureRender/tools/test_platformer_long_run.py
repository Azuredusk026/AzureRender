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

class CheckpointSetup(unittest.TestCase):
 def harness(self):
  h=BackgroundScanHarness();h.state['loading']=False;h.state['checkpointActivated']=True
  h.index=4;h.checkpointTested=False;h.phase='land';h.phaseStarted=25.
  h.state['tasks'][0]['collected']=4;h.state['tasks'][0]['completed']=False
  hero=h.state['characters'][0];hero['position']=[-.94602567,h.platforms[4]['top'],h.platforms[4]['z']-.032475]
  hero['grounded']=True;h.keys=set();return h
 def test_side_landing_prepares_checkpoint_before_intentional_fall(self):
  h=self.harness();h.advance(h.state,25.55)
  self.assertEqual(h.phase,'checkpoint-align');self.assertFalse(h.checkpointTested)
  self.assertFalse(h.keys)
 def test_pending_checkpoint_coin_cannot_set_recovery_baseline(self):
  h=self.harness();h.phase='checkpoint-align';h.state['characters'][0]['position'][0]=0.
  h._update(25.7);self.assertEqual(h.phase,'checkpoint-align');self.assertFalse(h.keys)
  self.assertFalse(h.checkpointTested)
 def test_alignment_moves_towards_center_and_saves_collected_coin(self):
  h=self.harness();h.phase='checkpoint-align';h._update(25.7);self.assertIn(68,h.keys)
  h.state['characters'][0]['position'][0]=-.1;h.state['tasks'][0]['collected']=5
  h._update(25.9);self.assertEqual(h.phase,'fall-drive');self.assertTrue(h.checkpointTested)
  self.assertEqual(h.savedCoins,5);self.assertTrue(h.checkpointFall)
 def test_alignment_from_opposite_side_uses_left_input(self):
  h=self.harness();h.phase='checkpoint-align';h.state['characters'][0]['position'][0]=.7
  h._update(25.7);self.assertIn(65,h.keys);self.assertNotIn(68,h.keys)
 def test_airborne_checkpoint_cannot_start_recovery_test(self):
  h=self.harness();h.phase='checkpoint-align';h.state['characters'][0]['position'][0]=0.
  h.state['characters'][0]['grounded']=False;h.state['tasks'][0]['collected']=5
  h._update(25.7);self.assertEqual(h.phase,'checkpoint-align');self.assertFalse(h.checkpointTested)

observedLanding=json.loads((root/'tests/fixtures/platformer-landing-overshoot.json').read_text(encoding='utf-8'))
class LandingAlignment(unittest.TestCase):
    def harness(self):
        h=BackgroundScanHarness()
        h.state=copy.deepcopy(observedLanding['value'])
        h.phase='land';h.phaseStarted=observedLanding['seconds']-1;h.keys=set()
        return h
    def update(self,h):h._update(observedLanding['seconds'])
    def test_recorded_supported_stop_enters_correction_without_counting(self):
        h=self.harness();self.update(h)
        self.assertEqual(h.phase,'landing-align')
        self.assertEqual(h.counts['mandatoryLandings'],0)
    def test_correction_uses_backward_input_for_past_center(self):
        h=self.harness();h.phase='landing-align';self.update(h)
        self.assertIn(83,h.keys);self.assertNotIn(87,h.keys)
        self.assertEqual(h.counts['mandatoryLandings'],0)
    def test_correction_uses_forward_input_from_opposite_side(self):
        h=self.harness();h.phase='landing-align'
        h.state['characters'][0]['position'][2]=h.platforms[1]['z']+1
        self.update(h);self.assertIn(87,h.keys);self.assertNotIn(83,h.keys)
    def test_corrected_stop_waits_for_braking_before_original_landing_check(self):
        h=self.harness();h.phase='landing-align';h.keys={83}
        hero=h.state['characters'][0];hero['position'][2]=h.platforms[1]['z']-.1
        hero['velocity'][2]=1.;self.update(h)
        self.assertFalse(h.keys);self.assertEqual(h.phase,'landing-align')
        self.assertEqual(h.counts['mandatoryLandings'],0)
        hero['velocity'][2]=0.;self.update(h)
        self.assertEqual(h.phase,'land');self.assertEqual(h.counts['mandatoryLandings'],0)
        self.update(h);self.assertEqual(h.counts['mandatoryLandings'],1)
    def test_airborne_or_lower_surface_cannot_begin_correction(self):
        for airborne in (True,False):
            h=self.harness();hero=h.state['characters'][0]
            if airborne:hero['grounded']=False
            else:hero['position'][1]=h.platforms[1]['top']-.2
            self.update(h);self.assertEqual(h.phase,'land');self.assertFalse(h.keys)
    def test_ground_outside_target_surface_cannot_begin_correction(self):
        h=self.harness();h.state['characters'][0]['position'][2]=h.platforms[1]['z']-h.platforms[1]['halfLength']-.1
        self.update(h);self.assertEqual(h.phase,'land');self.assertFalse(h.keys)
    def test_completed_goal_does_not_receive_corrective_movement(self):
        h=self.harness();h.state['tasks'][0]['completed']=True
        self.update(h);self.assertEqual(h.phase,'land');self.assertFalse(h.keys)
    def test_correction_releases_input_if_support_or_height_is_lost(self):
        for mode in ('airborne','lower','outside','completed'):
            h=self.harness();h.phase='landing-align';h.keys={83}
            hero=h.state['characters'][0]
            if mode=='airborne':hero['grounded']=False
            elif mode=='lower':hero['position'][1]=h.platforms[1]['top']-.2
            elif mode=='outside':hero['position'][2]=h.platforms[1]['z']-h.platforms[1]['halfLength']-.1
            else:h.state['tasks'][0]['completed']=True
            self.update(h);self.assertFalse(h.keys,msg=mode)
            self.assertEqual(h.phase,'land',msg=mode)
            self.assertEqual(h.counts['mandatoryLandings'],0)
    def test_reversing_correction_releases_old_direction_first(self):
        h=self.harness();h.phase='landing-align';h.keys={83}
        h.state['characters'][0]['position'][2]=h.platforms[1]['z']+1
        self.update(h)
        self.assertEqual(h.keys,{87})
        self.assertEqual(h.events[-2:],[
            dict(action='key',key=83,down=False),dict(action='key',key=87,down=True)])

observedFall=json.loads((root/'tests/fixtures/platformer-observed-fall.json').read_text(encoding='utf-8'))
class ObservedFallBoundary(unittest.TestCase):
    def harness(self,checkpoint=False):
        h=BackgroundScanHarness();h.state=copy.deepcopy(observedFall['value'])
        h.phase='fall-drive';h.phaseStarted=observedFall['phaseStarted'];h.keys={68}
        h.checkpointFall=checkpoint;h.sawFall=False;h.savedCoins=0
        if checkpoint:
            h.savedCoins=5;h.state['tasks'][0]['collected']=5
            h.state['characters'][0]['position'][1]=h.platforms[4]['top']-1.1
        return h
    def update(self,h):h._update(observedFall['seconds'])
    def recover(self,h):
        hero=h.state['characters'][0];p=h.platforms[4 if h.checkpointFall else 0]
        hero['position']=[0.,p['top'],p['z']];hero['grounded']=True
    def test_observed_fall_releases_input_before_cutoff_without_counting_recovery(self):
        h=self.harness();self.assertLess(observedFall['seconds']-h.phaseStarted,1.4)
        self.update(h)
        self.assertTrue(h.sawFall);self.assertFalse(h.keys)
        self.assertEqual(h.events,[dict(action='key',key=68,down=False)])
        self.assertEqual(h.phase,'fall-recover');self.assertEqual(h.counts['startFallRecoveries'],0)
    def test_observed_fall_and_centered_grounded_return_count_once(self):
        for checkpoint in (False,True):
            h=self.harness(checkpoint);self.update(h);self.recover(h)
            self.update(h);self.assertEqual(h.phase,'walk')
            key='checkpointFallRecoveries' if checkpoint else 'startFallRecoveries'
            self.assertEqual(h.counts[key],1)
    def test_airborne_or_off_center_return_cannot_count_recovery(self):
        for condition in ('airborne','side','route'):
            h=self.harness();self.update(h);self.recover(h)
            hero=h.state['characters'][0]
            if condition=='airborne':hero['grounded']=False
            elif condition=='side':hero['position'][0]=.41
            else:hero['position'][2]=.41
            self.update(h);self.assertEqual(h.counts['startFallRecoveries'],0,msg=condition)
            self.assertEqual(h.phase,'fall-recover',msg=condition)
    def test_checkpoint_recovery_requires_preserved_coins(self):
        h=self.harness(True);self.update(h);self.recover(h)
        h.state['tasks'][0]['collected']=4
        with self.assertRaisesRegex(AssertionError,'Recovery must preserve coins'):self.update(h)
        self.assertEqual(h.counts['checkpointFallRecoveries'],0)
    def test_duration_fallback_releases_input_without_inventing_fall_evidence(self):
        h=self.harness();self.recover(h);h._update(h.phaseStarted+1.5)
        self.assertEqual(h.phase,'fall-recover');self.assertFalse(h.keys);self.assertFalse(h.sawFall)
        h._update(h.phaseStarted+.1);self.assertEqual(h.counts['startFallRecoveries'],0)
    def test_grounded_position_alone_cannot_prove_fall_recovery(self):
        h=self.harness();h.phase='fall-recover';self.recover(h)
        self.update(h);self.assertEqual(h.counts['startFallRecoveries'],0)
        self.assertEqual(h.phase,'fall-recover')


import threading
import tempfile
from unittest.mock import patch
from long_run_telemetry import LiveGpuTiming, AsyncProgressWriter

class LongRunTelemetryRules(unittest.TestCase):
    def test_incremental_csv_preserves_partial_rows_and_never_duplicates_frames(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'timing.csv'
            with LiveGpuTiming(path) as reader:
                self.assertEqual(reader.read(),[])
                path.write_bytes(b'frame,frameMs\n1,3.25\n2,4.')
                self.assertEqual(reader.read(),[3.25]);self.assertEqual(reader.read(),[])
                with path.open('ab') as writer:writer.write(b'5\n3,6.75\n')
                self.assertEqual(reader.read(),[4.5,6.75]);self.assertEqual(reader.read(),[])
            path.unlink();self.assertFalse(path.exists())
    def test_progress_publish_returns_while_storage_is_blocked_and_close_flushes(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'progress.json';entered=threading.Event();release=threading.Event()
            returned=threading.Event();original=Path.write_text;errors=[]
            def delayed(target,*args,**kwargs):
                entered.set();release.wait(5);return original(target,*args,**kwargs)
            writer=AsyncProgressWriter(path);record={'seconds':60,'working':1234}
            def publish():
                try:writer.publish(record)
                except BaseException as error:errors.append(error)
                finally:returned.set()
            with patch.object(Path,'write_text',delayed):
                publisher=threading.Thread(target=publish);publisher.start()
                try:
                    self.assertTrue(entered.wait(2));self.assertTrue(returned.wait(.5))
                    record['seconds']=999
                    self.assertFalse(path.exists())
                finally:
                    release.set();publisher.join(3);writer.close()
            self.assertFalse(errors);self.assertFalse(publisher.is_alive())
            self.assertEqual(json.loads(path.read_text(encoding='utf-8')),{'seconds':60,'working':1234})
    def test_progress_write_failure_propagates_when_its_owner_closes(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(OSError):
                with AsyncProgressWriter(Path(directory)/'absent'/'progress.json') as writer:
                    writer.publish({'seconds':60})
    def test_invalid_complete_csv_row_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'timing.csv';path.write_bytes(b'frame,frameMs\n1,bad\n')
            with LiveGpuTiming(path) as reader:
                with self.assertRaises(ValueError):reader.read()


observedFocus=json.loads((root/'tests/fixtures/platformer-focus-loss.json').read_text(encoding='utf-8'))
class LiveFocusRecovery(unittest.TestCase):
    def harness(self):
        h=BackgroundScanHarness();h.state=copy.deepcopy(observedFocus['value'])
        h.phase='walk';h.phaseStarted=observedFocus['seconds']-1;h.index=0;h.keys={87}
        return h
    def test_recorded_focus_loss_reacquires_focus_and_replays_held_keys(self):
        h=self.harness();started=h.phaseStarted;h._update(observedFocus['seconds'])
        self.assertEqual(h.events[:2],[dict(action='focus',focused=True),dict(action='key',key=87,down=True)])
        self.assertEqual(h.phaseStarted,started);self.assertEqual(h.counts['mandatoryLandings'],0)
    def test_focused_state_keeps_existing_key_edge_semantics(self):
        h=self.harness();h.state['focused']=True;h._update(observedFocus['seconds'])
        self.assertFalse(h.events)
    def test_recovery_preserves_route_timeout(self):
        h=self.harness();h.phaseStarted=observedFocus['seconds']-46
        with self.assertRaisesRegex(AssertionError,'Timed out'):h._update(observedFocus['seconds'])
    def test_lost_focus_does_not_invent_fall_or_landing(self):
        h=self.harness();h.phase='fall-recover';h.checkpointFall=False;h.sawFall=False;h.savedCoins=0
        h._update(observedFocus['seconds'])
        self.assertEqual(h.counts['startFallRecoveries'],0);self.assertEqual(h.counts['mandatoryLandings'],0)

class ShutdownTransitionAccounting(unittest.TestCase):
    def harness(self, phase='restart'):
        h=PlatformerScenario();h.phase=phase;h.oldRevision=24
        h.oldReference=h.course;h.desired=h.exploration
        h.revisions={24:h.course};h.event=lambda *args,**fields:None
        rows=[dict(frame=9840,revision=24,loading=False,committed=False,
                   buffers=84,images=323,bufferBytes=22497392,imageBytes=330748416),
              dict(frame=9913,revision=25,loading=False,committed=True,
                   buffers=84,images=323,bufferBytes=22497392,imageBytes=330748416)]
        return h,rows
    def test_restart_committed_during_shutdown_is_accounted(self):
        h,rows=self.harness();result=h.finalize(rows,0)
        self.assertEqual(result['restarts'],1)
        self.assertEqual(result['revisionReferences'][25],h.course)
    def test_requested_level_committed_during_shutdown_is_accounted(self):
        h,rows=self.harness('loading');result=h.finalize(rows,0)
        self.assertEqual(result['switches'],1)
        self.assertEqual(result['revisionReferences'][25],h.exploration)
    def test_unowned_shutdown_commit_is_rejected(self):
        h,rows=self.harness('walk')
        with self.assertRaises(AssertionError):h.finalize(rows,0)
    def test_repeated_finalization_preserves_exact_counts(self):
        h,rows=self.harness();h.finalize(rows,0);result=h.finalize(rows,0)
        self.assertEqual(result['restarts'],1)
    def test_finalization_uses_receipt_after_route_journal_is_closed(self):
        import io
        h,rows=self.harness();h.event=PlatformerScenario.event.__get__(h)
        h.journal=io.StringIO();h.journal.close()
        result=h.finalize(rows,0)
        self.assertEqual(result['shutdownTransition']['frame'],9913)

if __name__=='__main__':unittest.main()
