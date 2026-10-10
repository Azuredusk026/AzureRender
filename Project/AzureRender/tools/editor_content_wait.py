"""Wait for the public content snapshot after an external directory change."""
import time


def wait_content_scan(workspace, advance_frames, previous_enumerations, timeout=10):
    deadline = time.monotonic() + timeout
    while True:
        state = workspace()
        statistics = state['contentNavigation']['statistics']
        if (statistics['directoryEnumerations'] > previous_enumerations
                and not statistics['refreshing']):
            return state
        if time.monotonic() >= deadline:
            raise TimeoutError('External content scan did not complete')
        advance_frames()
