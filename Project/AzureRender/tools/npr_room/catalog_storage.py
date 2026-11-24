"""Publish a verified catalog from a same-volume staging directory."""

import time


def publish_directory(staged, output):
    deadline = time.monotonic() + 2
    while True:
        if output.exists():
            raise FileExistsError(output)
        try:
            staged.rename(output)
            return
        except PermissionError as error:
            if getattr(error, "winerror", None) not in (5, 32, 33) or time.monotonic() >= deadline:
                raise
            time.sleep(0.05)
