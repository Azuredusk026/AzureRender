"""Exercise installed project creation, reopen, save and publishing via native UI input."""
import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import secrets
from PIL import ImageGrab
from run_playable_long_run import NativeWindow
from test_validation_protocol import exchange


class NativeUI:
    def __init__(self, executable, config, output, replay=False):
        self.output = output
        output.mkdir(parents=True, exist_ok=True)
        self.report = output / 'ui.json'
        self.report.unlink(missing_ok=True)
        self.actions = []
        self.last_state = {}
        self.replay = replay
        self.center_events = []
        self.center_input = output / 'center-input.json'
        self.endpoint_path = output / 'endpoint.json'
        self.endpoint_path.unlink(missing_ok=True)
        self.token = secrets.token_hex(32)
        self.log = (output / 'process.log').open('w', encoding='utf-8')
        env = dict(os.environ, AZURERENDER_EDITOR_CONFIG=str(config),
                   AZURERENDER_UI_OBSERVATION=str(self.report))
        env['PATH'] = str(Path(os.environ['SystemRoot']) / 'System32')
        env.pop('AZURERENDER_RESOURCE_ROOT', None)
        if replay:
            self.center_input.write_text('[]', encoding='utf-8')
            env.update(AZURERENDER_PROJECT_CENTER_UI_ACTIONS=str(self.center_input),
                       AZURERENDER_STARTUP_VALIDATION_ENDPOINT=str(self.endpoint_path),
                       AZURERENDER_STARTUP_VALIDATION_TOKEN=self.token, AZURERENDER_EDITOR_DPI='1')
        self.process = subprocess.Popen([str(executable)], cwd=output, env=env,
                                        stdout=self.log, stderr=self.log)
        self.window = NativeWindow(self.process.pid)
        self.window.user.ClientToScreen.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
        self.window.user.SetForegroundWindow.argtypes = [wintypes.HWND]
        self.window.user.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
        self.window.user.GetCursorPos.argtypes = [ctypes.POINTER(wintypes.POINT)]
        self.cursor = wintypes.POINT()
        self.window.user.GetCursorPos(ctypes.byref(self.cursor))

    def state(self):
        try:
            self.last_state = json.loads(self.report.read_text(encoding='utf-8'))
        except (OSError, ValueError):
            pass
        return self.last_state

    def until(self, predicate, timeout=45):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            state = self.state()
            if predicate(state):
                return state
            assert self.process.poll() is None, (self.process.returncode, self.output)
            time.sleep(.05)
        raise AssertionError(('UI wait timed out', self.state(), self.output))

    def __enter__(self):
        try:
            self.until(lambda s: s.get('phase') == 'ProjectCenter')
        except BaseException:
            self.__exit__(True)
            raise
        return self

    def post(self, message, wparam=0, lparam=0):
        assert self.window.find(), 'Owned window must exist'
        assert self.window.user.PostMessageW(self.window.handle, message, wparam, lparam)
        self.actions.append(dict(message=message, wparam=wparam, lparam=lparam))
        time.sleep(.08)

    def key(self, code, down=True):
        scan = self.window.user.MapVirtualKeyW(code, 0)
        self.post(0x100 if down else 0x101, code,
                  1 | (scan << 16) | (0 if down else (1 << 30) | (1 << 31)))

    def click(self, target):
        if self.replay:
            return self.replay_click(target)
        state = self.until(lambda s: target in s.get('widgets', {}))
        panel = 'inspector' if target == 'name' else 'build' if target.startswith(('output.', 'install.', 'project.build')) else None
        if panel:
            for attempt in range(15):
                rect = state['widgets'][target]
                bounds = state['panels'][panel]['rect']
                if bounds[1] + 35 < rect[1] and rect[1] + rect[3] < bounds[1] + bounds[3] - 8:
                    break
                assert self.window.find()
                point = wintypes.POINT(int(bounds[0] + bounds[2] / 2), int(bounds[1] + bounds[3] / 2))
                assert self.window.user.ClientToScreen(self.window.handle, ctypes.byref(point))
                self.window.user.SetCursorPos(point.x, point.y)
                delta = -120 if rect[1] > bounds[1] + 35 else 120
                self.post(0x20A, (delta & 0xffff) << 16, (point.x & 0xffff) | ((point.y & 0xffff) << 16))
                time.sleep(.25); state = self.state()
            else:
                raise AssertionError(('Cannot scroll to widget', target, rect, bounds))
        rect = state['widgets'][target]
        x, y = int(rect[0] + rect[2] / 2), int(rect[1] + rect[3] / 2)
        assert self.window.find()
        self.window.user.SetForegroundWindow(self.window.handle)
        self.post(0x7)
        point = wintypes.POINT(x, y)
        assert self.window.user.ClientToScreen(self.window.handle, ctypes.byref(point))
        cursor_moved = bool(self.window.user.SetCursorPos(point.x, point.y))
        self.actions.append(dict(cursorMoved=cursor_moved))
        packed = (x & 0xffff) | ((y & 0xffff) << 16)
        self.post(0x200, 0, packed)
        self.post(0x201, 1, packed)
        self.post(0x202, 0, packed)
        time.sleep(.25)
        self.actions.append(dict(widget=target))

    def fill(self, target, text):
        if self.replay:
            self.click(target)
            events = [dict(action='key', key='A', down=True, ctrl=True),
                      dict(action='key', key='A', down=False, ctrl=True),
                      dict(action='key', key='A', down=False, ctrl=False),
                      dict(action='text', text=text)]
            for event in events:
                if self.state()['phase'] == 'ProjectCenter': self.center_event(event)
                else: self.editor_event(event)
            return
        self.click(target)
        self.key(0x11); self.key(ord('A')); self.key(ord('A'), False); self.key(0x11, False)
        # WM_CHAR reaches the GLFW character callback and the ImGui input queue.
        for char in text:
            self.post(0x102, ord(char))
        time.sleep(.25)

    def snapshot(self, label):
        state = self.state()
        (self.output / (label + '.json')).write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')
        assert self.window.find()
        self.window.user.SetForegroundWindow(self.window.handle)
        time.sleep(.3)
        rect = wintypes.RECT()
        self.window.user.GetWindowRect(self.window.handle, ctypes.byref(rect))
        try:
            ImageGrab.grab((rect.left, rect.top, rect.right, rect.bottom)).save(self.output / (label + '.png'))
        except OSError as error:
            self.actions.append(dict(screenshotUnavailable=str(error)))

    def close(self):
        assert self.window.find()
        self.window.close()
        assert self.process.wait(timeout=30) == 0

    def editor_event(self, event):
        endpoint = json.loads(self.endpoint_path.read_text(encoding='utf-8'))
        response = exchange(endpoint, self.token, dict(op='input', event=event))
        assert response['passed'], response
        response = exchange(endpoint, self.token, dict(op='wait-frames', frames=18))
        assert response['passed'], response
        self.actions.append(dict(replay=event))

    def center_event(self, event):
        frame = self.state()['frame'] + 24
        self.center_events.append(dict(frame=frame, **event))
        temporary = self.center_input.with_suffix('.pending')
        temporary.write_text(json.dumps(self.center_events, ensure_ascii=False), encoding='utf-8')
        deadline = time.monotonic() + 3
        while True:
            try:
                temporary.replace(self.center_input); break
            except PermissionError:
                if time.monotonic() >= deadline: raise
                time.sleep(.01)
        self.until(lambda s: s.get('phase') != 'ProjectCenter' or s.get('frame', 0) >= frame + 6)
        self.actions.append(dict(replay=event))

    def replay_click(self, target):
        state = self.until(lambda s: target in s.get('widgets', {}))
        if state['phase'] == 'ProjectCenter':
            self.center_event(dict(action='click', target=target))
            return
        panel = 'inspector' if target == 'name' else 'build' if target.startswith(('output.', 'install.', 'project.build')) else None
        if panel:
            for _ in range(20):
                state = self.state(); rect = state['widgets'][target]; bounds = state['panels'][panel]['rect']
                if bounds[1] + 25 < rect[1] and rect[1] + rect[3] < bounds[1] + bounds[3] - 8: break
                self.editor_event(dict(action='mouse', target='panelrect.' + panel, down=False))
                self.editor_event(dict(action='wheel', y=-2 if rect[1] > bounds[1] + 25 else 2))
            else: raise AssertionError(('Cannot scroll to widget', target, rect, bounds))
        self.editor_event(dict(action='mouse', target=target, down=False))
        self.editor_event(dict(action='click', target=target))

    def __exit__(self, *error):
        try:
            if self.process.poll() is None:
                if self.window.find(): self.window.close()
                try: self.process.wait(timeout=5)
                except subprocess.TimeoutExpired: self.process.terminate(); self.process.wait(timeout=10)
        finally:
            self.log.close()
            self.window.user.SetCursorPos(self.cursor.x, self.cursor.y)
            (self.output / 'native-input.json').write_text(json.dumps(self.actions, indent=2), encoding='utf-8')
        if not any(error):
            assert self.process.returncode == 0
            log = (self.output / 'process.log').read_text(encoding='utf-8', errors='replace')
            assert 'VUID-' not in log and 'Validation Error' not in log, log[-3000:]


def edit_name(ui, name):
    widgets = ui.until(lambda s: s.get('phase') == 'Editor')['widgets']
    target = next((k for k in ('node.hero:body', 'node.hero') if k in widgets), next(k for k in widgets if k.startswith('node.')))
    ui.click(target)
    ui.fill('name', name)
    ui.click('save')
    ui.until(lambda s: s.get('selectedName') == name and not s.get('dirty', True))


def run(install, output, replay=False):
    ctypes.WinDLL('user32').SetProcessDPIAware()
    install = install.resolve(); output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='Azure GUI 中文 '))
    config = root / '用户配置'
    executable = install / 'bin/AzureRender.exe'
    project = root / 'Monica/project.azureproject'
    with NativeUI(executable, config, output / 'create', replay) as ui:
        ui.snapshot('project-center')
        ui.click('center.new.tab')
        ui.fill('center.name', 'Monica')
        ui.fill('center.parent', str(root))
        ui.snapshot('create-form')
        ui.click('center.create')
        ui.until(lambda s: s.get('phase') == 'Editor')
        ui.window.find();ui.window.resize(1920,1080);time.sleep(.5)
        assert project.is_file()
        edit_name(ui, '初次保存')
        ui.snapshot('saved-editor')
        ui.close()
    metadata = json.loads(project.read_text(encoding='utf-8'))
    published = root / 'GUI 发布 Monica'
    with NativeUI(executable, config, output / 'reopen', replay) as ui:
        ui.click('center.recent.' + metadata['id'])
        ui.until(lambda s: s.get('phase') == 'Editor')
        ui.window.find();ui.window.resize(1920,1080);time.sleep(.5)
        edit_name(ui, 'Monica 主角')
        ui.fill('name', '未保存修改')
        ui.window.close()
        ui.until(lambda s: 'document.cancel' in s.get('widgets', {}))
        ui.click('document.cancel')
        ui.until(lambda s: s.get('dirty') and s.get('documentGuard') == 0)
        ui.click('save')
        ui.until(lambda s: not s.get('dirty', True))
        ui.click('menu.Tools'); ui.click('tool.build')
        if 'project.build' not in ui.state().get('widgets', {}): ui.click('tab.build')
        assert Path(ui.state()['build']['install']) == install
        ui.fill('output.path', str(published))
        ui.click('project.build')
        state = ui.until(lambda s: s.get('build', {}).get('passed') is True, timeout=120)
        assert not state['build']['running']
        ui.snapshot('gui-published')
        ui.close()
    moved = root / '移动后的 GUI 发布'
    published.rename(moved)
    # Hide the authoring project to establish that runtime uses only its package.
    project.parent.rename(root / '源项目暂存')
    env = dict(os.environ, PATH=str(Path(os.environ['SystemRoot']) / 'System32'))
    env.pop('AZURERENDER_RESOURCE_ROOT', None)
    result = subprocess.run([str(moved / 'Monica.exe'), '--no-error-dialog', '--smoke-frames', '60',
                             '--runtime-report', str(output / 'game-runtime.json')],
                            cwd=output, env=env, timeout=90)
    assert result.returncode == 0, (moved / 'captures/game-startup.log').read_text(encoding='utf-8', errors='replace')
    runtime = json.loads((output / 'game-runtime.json').read_text(encoding='utf-8'))
    assert runtime.get('steps', 0) > 0 or runtime.get('fixedSteps', 0) > 0
    summary = dict(passed=True, install=str(install), root=str(root), package=str(moved),
                   guiCreate=True, guiReopen=True, guiEditSave=True, unsavedCloseCancel=True,
                   guiPublish=True, movedNativeEntry=True, sourceProjectUnavailable=True,
                   input='production Dear ImGui replay' if replay else 'native Windows messages through GLFW and Dear ImGui', physicalInput='not performed')
    (output / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--install', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--replay', action='store_true')
    args = parser.parse_args()
    run(args.install, args.output, args.replay)
