"""Preserve observations and reap only the process owned by a run."""
import json
import subprocess


def finalize_run(process, window, output, observations, scenario=None,
                 *, grace_seconds=20):
    report = dict(forcedTermination=False, normalExit=False, exitCode=None,
                  errors=[])

    def record_error(operation, error):
        report['errors'].append(f'{operation}: {type(error).__name__}: {error}')

    # Write before any cleanup request; a stalled host must not lose samples.
    try:
        (output/'observation.json').write_text(
            json.dumps(observations, indent=2)+'\n', encoding='utf-8')
    except Exception as error:
        record_error('observation', error)

    try:
        if process.poll() is None:
            if window.handle:
                try:
                    # WM_CLOSE is posted asynchronously. Window restoration is
                    # a separate live-run check, never a failure cleanup step.
                    window.close()
                except Exception as error:
                    record_error('close-request', error)
                try:
                    process.wait(timeout=grace_seconds)
                except subprocess.TimeoutExpired:
                    report['forcedTermination'] = True
                    process.kill()
                    process.wait(timeout=5)
            else:
                report['forcedTermination'] = True
                process.kill()
                process.wait(timeout=5)
    except Exception as error:
        record_error('process-cleanup', error)

    report['exitCode'] = process.poll()
    report['normalExit'] = report['exitCode'] == 0 and not report['forcedTermination']
    if scenario:
        try:
            scenario.close()
        except Exception as error:
            record_error('scenario-close', error)
    (output/'cleanup.json').write_text(
        json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return report
