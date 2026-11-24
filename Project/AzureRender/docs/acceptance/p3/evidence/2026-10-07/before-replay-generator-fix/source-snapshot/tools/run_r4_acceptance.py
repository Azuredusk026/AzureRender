"""Sequential Windows R4 performance, capture and lifecycle acceptance."""
import argparse
import ctypes
from ctypes import wintypes
import json
import hashlib
import math
from pathlib import Path
import re
import subprocess
import time

from PIL import Image, ImageStat

ROOT = Path(__file__).resolve().parents[1]
STRESS = ROOT / "assets_public/scenes/r4_stress.azscene"


def validate_report(report, frames, mode, instances=None):
    submission = report["submission"]
    if report["samples"] != frames or submission["frames"] != frames or frames < 1:
        raise ValueError("Incomplete GPU timing samples")
    for value in (report["totalAverageMs"], report["totalP95Ms"], report["totalP99Ms"],
                  submission["recordingMilliseconds"]):
        if not math.isfinite(value) or value < 0:
            raise ValueError("Invalid timing sample")
    if instances is not None:
        if submission["instances"] != instances * frames:
            raise ValueError("Incorrect workload instance count")
        parallel = mode.startswith("parallel")
        if (submission["workerRecordedPasses"] > 0) != parallel:
            raise ValueError("Incorrect recording path")
        if (submission["indirectDrawCalls"] > 0) != mode.endswith("gpu"):
            raise ValueError("Incorrect visibility path")


def validate_lifecycle(log, cycles, blackhole):
    if "VUID-" in log or "Validation Error" in log:
        raise ValueError("Vulkan validation error")
    if log.count("Swapchain recreated") < cycles:
        raise ValueError("Missing observed swapchain recreations")
    if blackhole and log.count("Blackhole history reset after recreate") < cycles:
        raise ValueError("Missing observed temporal history resets")
    if "Allocator after unload: buffers=0 images=0" not in log:
        raise ValueError("GPU allocations survived unload")
    if "Screenshot saved:" not in log:
        raise ValueError("Missing runtime screenshot")


def verify_image(path):
    with Image.open(path) as image:
        image.load()
        if min(image.size) < 1 or max(ImageStat.Stat(image.convert("RGB")).var) == 0:
            raise ValueError("Empty or constant screenshot")


def validate_capture_hashes(reference, candidate):
    if len(reference) != 2 or candidate != reference:
        raise ValueError("Complex scene capture paths differ")


def run(executable, arguments, output, name, timeout=180):
    result = subprocess.run([str(executable), *arguments], cwd=ROOT, capture_output=True,
                            encoding="utf-8", errors="replace", timeout=timeout)
    log = result.stdout + result.stderr
    (output / f"{name}.log").write_text(log, encoding="utf-8")
    if result.returncode or "VUID-" in log or "Validation Error" in log:
        raise RuntimeError(f"{name} failed; inspect its log")
    if "Allocator after unload: buffers=0 images=0" not in log:
        raise ValueError(f"{name}: missing clean allocator unload")


def performance(executable, output):
    results = {}
    cases = [(name, ["--scene", str(STRESS), *switches], 300, 32) for name, switches in (
        ("parallel_gpu", []), ("serial_gpu", ["--disable-parallel-recording"]),
        ("parallel_cpu", ["--disable-gpu-culling"]),
        ("serial_cpu", ["--disable-parallel-recording", "--disable-gpu-culling"]))]
    cases += [(f"blackhole_{quality}", ["--scene-type", "blackhole", "--blackhole-quality", quality],
               150, None) for quality in ("performance", "balanced", "cinematic")]
    for name, switches, frames, instances in cases:
        report = output / f"{name}.json"
        run(executable, ["--width", "1280", "--height", "720", "--fixed-frame-step",
                        "--smoke-frames", str(frames), "--gpu-timing", "--gpu-timing-output", str(report),
                        *switches], output, name)
        data = json.loads(report.read_text(encoding="utf-8"))
        validate_report(data, frames, name, instances)
        if not report.with_suffix(".csv").is_file():
            raise ValueError("Missing per-frame performance curve")
        results[name] = data
        print(f"{name}: {data['totalAverageMs']:.3f} ms GPU", flush=True)
    return results


def captures(executable, output):
    cases = [("stress", ["--scene", str(STRESS)]),
             ("stress_serial_gpu", ["--scene", str(STRESS), "--disable-parallel-recording"]),
             ("stress_parallel_cpu", ["--scene", str(STRESS), "--disable-gpu-culling"]),
             ("stress_serial_cpu", ["--scene", str(STRESS), "--disable-gpu-culling", "--disable-parallel-recording"])] + [
        (f"blackhole_{q}", ["--scene-type", "blackhole", "--blackhole-quality", q])
        for q in ("performance", "balanced", "cinematic")]
    reference = None
    for name, switches in cases:
        directory = output / f"capture-{name}"
        run(executable, ["--width", "1280", "--height", "720", "--capture-dir", str(directory),
                        "--capture-frames", "2", *switches], output, f"capture-{name}")
        images = sorted(directory.glob("*.png"))
        if len(images) != 2:
            raise ValueError("Incomplete capture sequence")
        for image in images:
            verify_image(image)
        if name.startswith("stress"):
            hashes = [hashlib.sha256(image.read_bytes()).hexdigest() for image in images]
            if reference is None:
                reference = hashes
            validate_capture_hashes(reference, hashes)
            (output / f"{name}-hashes.json").write_text(json.dumps(hashes), encoding="utf-8")
        manifest = json.loads((directory / "capture_manifest.json").read_text(encoding="utf-8"))
        if name.startswith("blackhole") and (not manifest["blackhole"]["historyValid"]
                                            or manifest["blackhole"]["historyResets"] < 1):
            raise ValueError("Temporal accumulation was not initialized")


def lifecycle(executable, output, scene, seconds, cycles):
    # Win32 operates only on windows owned by this subprocess.
    user = ctypes.windll.user32
    user.IsWindowVisible.argtypes = [wintypes.HWND]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
    user.MoveWindow.argtypes = [wintypes.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.BOOL]
    user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    callback = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    report = output / f"long-{scene}.json"
    log_path = output / f"long-{scene}.log"
    switches = ["--scene", str(STRESS)] if scene == "character" else [
        "--scene-type", "blackhole", "--blackhole-quality", "balanced"]
    with log_path.open("w", encoding="utf-8") as log_file:
        process = subprocess.Popen([str(executable), "--width", "1280", "--height", "720",
            "--gpu-timing", "--gpu-timing-output", str(report), *switches], cwd=ROOT,
            stdout=log_file, stderr=log_file)
        try:
            windows = []
            @callback
            def visit(hwnd, _):
                pid = wintypes.DWORD()
                user.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
                if pid.value == process.pid and user.IsWindowVisible(hwnd):
                    windows.append(hwnd)
                return True
            deadline = time.monotonic() + 30
            while not windows and time.monotonic() < deadline:
                user.EnumWindows(visit, 0)
                time.sleep(0.1)
            if not windows:
                raise RuntimeError("No renderer window")
            hwnd = windows[0]
            time.sleep(3)
            started = time.monotonic()
            for cycle in range(cycles):
                if not user.MoveWindow(hwnd, 50, 50, 900 + (cycle % 2) * 200, 650 + (cycle % 2) * 100, True):
                    raise RuntimeError("Window resize failed")
                time.sleep(1)
                user.ShowWindow(hwnd, 6)
                time.sleep(0.5)
                user.ShowWindow(hwnd, 9)
                time.sleep(1)
            # Deliver F12 to the owned window without changing the user's focus.
            scan = user.MapVirtualKeyW(0x7B, 0)
            user.PostMessageW(hwnd, 0x100, 0x7B, 1 | (scan << 16))
            user.PostMessageW(hwnd, 0x101, 0x7B, 1 | (scan << 16) | (3 << 30))
            while time.monotonic() - started < seconds:
                if process.poll() is not None:
                    raise RuntimeError("Renderer exited during sustained run")
                time.sleep(1)
            elapsed = time.monotonic() - started
            user.PostMessageW(hwnd, 0x10, 0, 0)
            if process.wait(timeout=30) != 0:
                raise RuntimeError("Renderer shutdown failed")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
    log = log_path.read_text(encoding="utf-8", errors="replace")
    validate_lifecycle(log, cycles, scene == "blackhole")
    match = re.search(r"Screenshot saved: (.+)", log)
    screenshot = Path(match.group(1).strip())
    if not screenshot.is_absolute():
        screenshot = ROOT / screenshot
    verify_image(screenshot)
    import shutil
    shutil.copyfile(screenshot, output / f"long-{scene}.png")
    data = json.loads(report.read_text(encoding="utf-8"))
    validate_report(data, data["samples"], "parallel_gpu", 32 if scene == "character" else None)
    if data["samples"] < 300:
        raise ValueError("Insufficient sustained rendering samples")
    return {"seconds": elapsed, "cycles": cycles, "recreations": log.count("Swapchain recreated"),
            "historyResets": log.count("Blackhole history reset after recreate"),
            "samples": data["samples"], "gpuMs": data["totalAverageMs"], "cleanUnload": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--mode", choices=("performance", "captures", "lifecycle", "all"), default="all")
    parser.add_argument("--seconds", type=int, default=120)
    parser.add_argument("--cycles", type=int, default=6)
    args = parser.parse_args()
    if args.seconds < 120 or args.cycles < 6:
        parser.error("R4 requires at least 120 seconds and six cycles")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("Use an empty output directory to preserve evidence")
    result = {"stage": "R4", "status": "running", "resolution": [1280, 720],
              "objects": 32, "lights": 16, "executable": str(args.executable.resolve())}
    try:
        if args.mode in ("performance", "all"):
            result["performance"] = performance(args.executable.resolve(), output)
        if args.mode in ("performance", "captures", "all"):
            captures(args.executable.resolve(), output)
        if args.mode in ("lifecycle", "all"):
            result["lifecycle"] = {}
            for scene in ("character", "blackhole"):
                result["lifecycle"][scene] = lifecycle(args.executable.resolve(), output, scene, args.seconds, args.cycles)
                print(f"{scene}: sustained lifecycle passed", flush=True)
        result["status"] = "passed"
    except Exception as error:
        result.update(status="failed", error=str(error))
        raise
    finally:
        (output / "summary.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
