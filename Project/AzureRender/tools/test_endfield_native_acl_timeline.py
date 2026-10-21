import math
import struct
import unittest

try:
    from endfield_native.acl_timeline import build_sample_plan
except ImportError:
    build_sample_plan = None


def stream(count=64, wrap=True, rate=60, version=10, track_type=0):
    return struct.pack('<IIIHBBIIfI', 32, 0, 0xac11ac11, version, 0, track_type,
                       28, count, rate, (1 << 30) if wrap else 0)


class AclTimelineTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(callable(build_sample_plan), 'Decoder timeline planner is required')
        self.buffers = dict(root_motion=stream(), float=stream(65, False),
                            transform=stream(track_type=12))

    def test_wrap_streams_have_an_evaluated_endpoint_without_truncation(self):
        result = build_sample_plan(self.buffers, source_start=0, source_stop=1.07098)
        self.assertEqual(result['format'], 'EndfieldAclTimeline/1')
        self.assertEqual(len(result['sample_times']), 65)
        self.assertAlmostEqual(result['sample_times'][-1], 64/60)
        self.assertEqual(result['streams']['root_motion']['stored_frames'], 64)
        self.assertEqual(result['streams']['float']['stored_frames'], 65)
        self.assertTrue(result['streams']['root_motion']['wrap_optimized'])
        self.assertFalse(result['streams']['float']['wrap_optimized'])
        self.assertFalse(result['game_time_mapping_verified'])
        self.assertEqual(result['source_clip_domain'], [0, 1.07098])

    def test_run_and_closed_nonwrap_streams_share_the_decoder_domain(self):
        self.buffers.update(root_motion=stream(40), float=stream(41, False),
                            transform=stream(41, False, track_type=12))
        result = build_sample_plan(self.buffers, source_start=0, source_stop=.67059)
        self.assertEqual(len(result['sample_times']), 41)
        self.assertAlmostEqual(result['decoder_duration'], 2/3)

    def test_version_seven_does_not_apply_the_new_wrap_flag(self):
        self.buffers = {k: stream(65, True, version=7, track_type=12 if k=='transform' else 0)
                        for k in self.buffers}
        self.assertAlmostEqual(build_sample_plan(self.buffers, source_start=0, source_stop=2)['decoder_duration'], 64/60)

    def test_reject_domain_and_rate_mismatch(self):
        for count, rate in [(66,60),(65,30)]:
            self.buffers['float']=stream(count,False,rate)
            with self.assertRaisesRegex(ValueError, 'duration|rate'):
                build_sample_plan(self.buffers, source_start=0, source_stop=2)

    def test_reject_invalid_headers_and_frame_budget(self):
        for value in [b'', stream()[:31], stream(rate=math.inf), stream(version=11),
                      stream(count=0), stream(track_type=12)]:
            self.buffers['float']=value
            with self.assertRaises(ValueError):
                build_sample_plan(self.buffers, source_start=0, source_stop=2)
        self.buffers['float']=stream(65,False)
        with self.assertRaisesRegex(ValueError, 'budget'):
            build_sample_plan(self.buffers, source_start=0, source_stop=2, maximum_frames=64)

    def test_reject_missing_stream_and_source_domain(self):
        for start, stop in [(1,1),(-1,1),(0,math.nan),(0,math.inf)]:
            with self.assertRaisesRegex(ValueError, 'source'):
                build_sample_plan(self.buffers, source_start=start, source_stop=stop)
        del self.buffers['root_motion']
        with self.assertRaisesRegex(ValueError, 'streams'):
            build_sample_plan(self.buffers, source_start=0, source_stop=1)


if __name__ == '__main__':
    unittest.main()
