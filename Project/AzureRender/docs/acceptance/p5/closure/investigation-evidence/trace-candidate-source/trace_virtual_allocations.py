"""Trace returned local virtual-memory calls in an owned Windows x64 process.

LLDB callbacks retain status and output values. This is a diagnostic trace,
not a complete heap ledger or a performance measurement.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time


def summarize_events(events):
    live = {}
    allocations = releases = failed = ambiguous = 0
    for event in events:
        if event['status'] & 0x80000000:
            failed += 1
            continue
        base, size, flags = event['base'], event['size'], event['flags']
        if event['action'] == 'allocate':
            allocations += 1
            if not flags & 0x2000 or not base or size <= 0 or base in live:
                ambiguous += 1
            else:
                live[base] = size
        elif event['action'] == 'release':
            releases += 1
            if not flags & 0x8000 or base not in live:
                ambiguous += 1
            else:
                del live[base]
        else:
            ambiguous += 1
    return dict(diagnosticOnly=True, successfulAllocations=allocations,
                successfulReleases=releases, failedCalls=failed,
                partialOrAmbiguousCalls=ambiguous, trackedLiveBytes=sum(live.values()),
                liveRegions=[dict(base=base, size=size) for base, size in live.items()],
                completeAllocationCoverage=False)


_state = None


class ReturnCalls:
    """Pair nested API returns by breakpoint address and native thread."""
    def __init__(self):self.calls={}
    def push(self,address,thread,call):
        self.calls.setdefault((address,thread),[]).append(call)
    def pop(self,address,thread):
        key=(address,thread)
        calls=self.calls.get(key)
        if not calls:return None
        value=calls.pop()
        if not calls:del self.calls[key]
        return value
    def pending(self):return [call for calls in self.calls.values() for call in calls]


def _read(process, address, size=8):
    import lldb
    error = lldb.SBError()
    data = process.ReadMemory(address, size, error)
    if not error.Success() or len(data) != size:
        raise RuntimeError('Unreadable trace argument at ' + hex(address))
    return int.from_bytes(data, 'little')


def _stack(thread):
    rows = []
    for frame in thread.frames[:24]:
        module = frame.GetModule().GetFileSpec()
        line = frame.GetLineEntry()
        rows.append(dict(address=frame.GetPC(), module=str(module),
                         function=frame.GetFunctionName() or '',
                         file=str(line.GetFileSpec()) if line.IsValid() else '',
                         line=line.GetLine() if line.IsValid() else 0))
    return rows


def entered(frame, bp_loc, internal_dict):
    """Capture output pointers and reuse a persistent return breakpoint."""
    state = _state
    try:
        if state['calls'] >= state['maxEvents']:
            state['droppedEntries'] += 1
            for breakpoint in state['entries']:
                breakpoint.SetEnabled(False)
            return False
        # This limited trace accepts the current-process pseudo handle only.
        if frame.FindRegister('rcx').GetValueAsUnsigned() != 0xffffffffffffffff:
            state['excludedNonPseudoHandles'] += 1
            return False
        process = frame.GetThread().GetProcess()
        stack_pointer = frame.FindRegister('rsp').GetValueAsUnsigned()
        api = state['apis'][bp_loc.GetBreakpoint().GetID()]
        base_pointer = frame.FindRegister('rdx').GetValueAsUnsigned()
        size_register = 'r9' if api == 'NtAllocateVirtualMemory' else 'r8'
        size_pointer = frame.FindRegister(size_register).GetValueAsUnsigned()
        flags = (_read(process, stack_pointer + 0x28, 4)
                 if api == 'NtAllocateVirtualMemory'
                 else frame.FindRegister('r9').GetValueAsUnsigned() & 0xffffffff)
        thread = frame.GetThread()
        call = dict(callId=state['calls'], api=api,
                    action='release' if api == 'NtFreeVirtualMemory' else 'allocate',
                    threadId=thread.GetThreadID(), basePointer=base_pointer,
                    sizePointer=size_pointer, base=_read(process, base_pointer),
                    size=_read(process, size_pointer), flags=flags,
                    entrySeconds=time.monotonic() - state['start'], stack=_stack(thread))
        address=_read(process,stack_pointer)
        if address not in state['returns']:
            breakpoint=process.GetTarget().BreakpointCreateByAddress(address)
            breakpoint.SetScriptCallbackFunction(__name__+'.returned')
            state['returns'][address]=breakpoint
            state['returnAddresses'][breakpoint.GetID()]=address
        state['pending'].push(address,thread.GetThreadID(),call)
        state['calls'] += 1
    except Exception as error:
        state['errors'].append(dict(stage='entry', message=str(error)))
    return False


def returned(frame, bp_loc, internal_dict):
    state = _state
    breakpoint = bp_loc.GetBreakpoint()
    identity = breakpoint.GetID()
    # Other threads can traverse a shared address without a traced API call.
    # Keep the trap installed so concurrent returns never race deletion.
    call=state['pending'].pop(state['returnAddresses'][identity],frame.GetThread().GetThreadID())
    if call is None:
        state['unmatchedReturns']+=1
        return False
    try:
        call['status'] = frame.FindRegister('rax').GetValueAsUnsigned() & 0xffffffff
        call['returnSeconds'] = time.monotonic() - state['start']
        if not call['status'] & 0x80000000:
            process = frame.GetThread().GetProcess()
            call['outputBase'] = _read(process, call['basePointer'])
            call['outputSize'] = _read(process, call['sizePointer'])
            if call['action'] == 'allocate':
                call['base'], call['size'] = call['outputBase'], call['outputSize']
        state['events'].append(call)
        state['stream'].write(json.dumps(call) + '\n')
        state['stream'].flush()
    except Exception as error:
        state['errors'].append(dict(stage='return', message=str(error), call=call))
    return False


def configure(debugger, config_path):
    global _state
    config = json.loads(Path(config_path).read_text(encoding='utf-8'))
    target = debugger.GetSelectedTarget()
    if target.GetAddressByteSize() != 8:
        raise RuntimeError('The trace requires a Windows x64 target')
    _state = dict(events=[], pending=ReturnCalls(), returns={},returnAddresses={},unmatchedReturns=0,
                  errors=[], entries=[], apis={}, calls=0,
                  droppedEntries=0, excludedNonPseudoHandles=0, start=time.monotonic(),
                  maxEvents=config['maxEvents'], output=Path(config['output']))
    _state['stream'] = (_state['output'] / 'events.jsonl').open('w', encoding='utf-8')
    for api in ('NtAllocateVirtualMemory', 'NtAllocateVirtualMemoryEx', 'NtFreeVirtualMemory'):
        breakpoint = target.BreakpointCreateByName(api, 'ntdll.dll')
        breakpoint.SetScriptCallbackFunction(__name__ + '.entered')
        _state['entries'].append(breakpoint)
        _state['apis'][breakpoint.GetID()] = api


def finish(debugger):
    state = _state
    state['stream'].close()
    result = summarize_events(state['events'])
    process = debugger.GetSelectedTarget().GetProcess()
    result.update(commandExitCode=process.GetExitStatus(), returnedEvents=len(state['events']),
                  entryCalls=state['calls'], pendingReturns=state['pending'].pending(),
                  sharedReturnSites=len(state['returns']),unmatchedReturns=state['unmatchedReturns'],
                  traceStartMonotonic=state['start'],
                  errors=state['errors'], droppedEntries=state['droppedEntries'],
                  excludedNonPseudoHandles=state['excludedNonPseudoHandles'],
                  resolvedApis={state['apis'][bp.GetID()]: bp.GetNumLocations()
                                for bp in state['entries']},
                  coverage='current-process pseudo handle; reserve/commit/decommit/release APIs',
                  limits=['Kernel heaps, mappings, explicit process handles and calls before debugger attachment are outside coverage.',
                          'Debugger timing and returned size do not establish resident ownership or a leak.'])
    (state['output'] / 'summary.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')


def trace(command, output, lldb_path, timeout, max_events):
    if not command:
        raise ValueError('An owned child command is required')
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    config = dict(output=str(output), command=command, maxEvents=max_events, diagnosticOnly=True)
    config['fileHashes'] = {str(path.resolve()): hashlib.sha256(path.read_bytes()).hexdigest()
                           for path in (Path(__file__), Path(command[0]), lldb_path)}
    config_path = output / 'config.json'
    config_path.write_text(json.dumps(config, indent=2) + '\n', encoding='utf-8')
    def quote(value):
        return '"' + str(value).replace('\\', '\\\\').replace('"', '\\"') + '"'
    commands = output / 'commands.lldb'
    commands.write_text('\n'.join([
        'target create ' + quote(command[0]),
        'command script import ' + quote(Path(__file__).resolve()),
        'process launch -s -o ' + quote(output / 'stdout.log') + ' -e ' + quote(output / 'stderr.log')
        + ' -- ' + ' '.join(quote(arg) for arg in command[1:]),
        'script trace_virtual_allocations.configure(lldb.debugger, ' + repr(str(config_path)) + ')',
        'continue', 'script trace_virtual_allocations.finish(lldb.debugger)', 'quit', '']), encoding='utf-8')
    info = subprocess.STARTUPINFO()
    info.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    info.wShowWindow = 0
    with (output / 'lldb.log').open('w', encoding='utf-8') as log:
        process = subprocess.Popen([str(lldb_path), '-b', '-s', str(commands)],
                                   stdout=log, stderr=subprocess.STDOUT, startupinfo=info)
        try:
            process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            # This PID was created here; the tree includes only our diagnostic target.
            subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'],
                           stdout=log, stderr=subprocess.STDOUT, check=False, startupinfo=info)
            process.wait(timeout=15)
            (output / 'error.json').write_text(json.dumps(dict(error='Owned trace exceeded timeout',
                                                              diagnosticOnly=True)) + '\n', encoding='utf-8')
            return 1
    summary_path = output / 'summary.json'
    if process.returncode or not summary_path.is_file():
        return 1
    result = json.loads(summary_path.read_text(encoding='utf-8'))
    return int(bool(result['errors'] or result['commandExitCode'] or result['pendingReturns']
                    or result['droppedEntries'] or not result['returnedEvents']
                    or not all(result['resolvedApis'].values())))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--lldb', type=Path, default=Path('C:/Program Files/LLVM/bin/lldb.exe'))
    parser.add_argument('--timeout', type=float, default=600)
    parser.add_argument('--max-events', type=int, default=100000)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.timeout <= 0 or args.max_events <= 0:
        parser.error('Timeout and event limit must be positive')
    raise SystemExit(trace(args.command[1:] if args.command[:1] == ['--'] else args.command,
                           args.output, args.lldb, args.timeout, args.max_events))
