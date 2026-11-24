"""Own append-only timing reads and progress writes outside input routing."""
from concurrent.futures import ThreadPoolExecutor
import json


class LiveGpuTiming:
    def __init__(self, path):
        self.path = path
        self.stream = None
        self.pending = b''

    def read(self):
        if self.stream is None:
            if not self.path.exists():
                return []
            self.stream = self.path.open('rb')
        self.pending += self.stream.read()
        lines = self.pending.split(b'\n')
        self.pending = lines.pop()
        return [float(line.split(b',')[-1]) for line in lines
                if line and not line.startswith(b'frame,')]

    def close(self):
        if self.stream is not None:
            self.stream.close()
            self.stream = None

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


class AsyncProgressWriter:
    def __init__(self, path):
        self.path = path
        self.worker = ThreadPoolExecutor(max_workers=1, thread_name_prefix='azure-progress')
        self.pending = None
        self.closed = False

    def publish(self, record):
        if self.closed:
            raise RuntimeError('Progress writer is closed')
        if self.pending is not None:
            if not self.pending.done():
                raise RuntimeError('Previous progress write is still pending')
            self.pending.result()
        text = json.dumps(record)
        self.pending = self.worker.submit(self.path.write_text, text, encoding='utf-8')

    def close(self):
        if self.closed:
            return
        self.closed = True
        try:
            if self.pending is not None:
                self.pending.result()
        finally:
            self.worker.shutdown(wait=True)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, *args):
        try:
            self.close()
        except Exception:
            if exc_type is None:
                raise
