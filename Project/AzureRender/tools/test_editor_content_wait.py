import unittest
from editor_content_wait import wait_content_scan

def snapshot(count,refreshing):
    return dict(contentNavigation=dict(statistics=dict(directoryEnumerations=count,refreshing=refreshing)))

class ContentScanWaitTests(unittest.TestCase):
    def test_requires_completed_scan_after_external_change(self):
        states=iter([snapshot(3,False),snapshot(3,True),snapshot(4,True),snapshot(4,False)])
        frames=[]
        result=wait_content_scan(lambda:next(states),lambda:frames.append('advance'),3)
        self.assertEqual(result['contentNavigation']['statistics']['directoryEnumerations'],4)
        self.assertFalse(result['contentNavigation']['statistics']['refreshing'])
        self.assertEqual(len(frames),3)
    def test_timeout_rejects_idle_snapshot_without_new_scan(self):
        with self.assertRaisesRegex(TimeoutError,'content scan'):
            wait_content_scan(lambda:snapshot(7,False),lambda:None,7,timeout=0)
    def test_does_not_swallow_query_failures(self):
        def failed():raise RuntimeError('Host disconnected')
        with self.assertRaisesRegex(RuntimeError,'Host disconnected'):
            wait_content_scan(failed,lambda:None,0)
if __name__=='__main__':unittest.main()
