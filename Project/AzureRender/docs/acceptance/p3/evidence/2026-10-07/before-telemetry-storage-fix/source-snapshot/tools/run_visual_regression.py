#!/usr/bin/env python3
"""Single entry point for the public GPU visual-regression gate.

Renders every case defined in `visual_regression_cases.json`, compares the
result against the committed baseline, and writes one machine-readable summary.

Modes:
  --update-baseline   Render and write baselines instead of comparing.
  --tolerance strict  Same device/driver comparison (default).
  --tolerance relaxed Software rasteriser or cross-device comparison.
  --ci-only           Restrict the run to cases marked ciEnabled.

Exit code is non-zero when any case fails, so the script can back a CTest
entry directly.
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Optional

import numpy as np
from PIL import Image

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_CASES = Path(__file__).resolve().parent / "visual_regression_cases.json"
DEFAULT_BASELINE_DIR = PROJECT_ROOT / "assets_public" / "baselines" / "character"


def run_renderer(command: list[str]) -> subprocess.CompletedProcess:
    if os.name != "nt":
        return subprocess.run(
            command,
            cwd=PROJECT_ROOT,
            capture_output=True,
            text=True,
        )

    with tempfile.TemporaryDirectory(prefix="azurerender-visual-") as directory:
        working_dir = Path(directory)
        launcher = working_dir / "run_minimized.ps1"
        standard_output = working_dir / "stdout.log"
        standard_error = working_dir / "stderr.log"
        launcher.write_text(
            "param(\n"
            "    [Parameter(Mandatory = $true)][string]$Executable,\n"
            "    [Parameter(Mandatory = $true)][string]$Arguments,\n"
            "    [Parameter(Mandatory = $true)][string]$StandardOutput,\n"
            "    [Parameter(Mandatory = $true)][string]$StandardError\n"
            ")\n"
            "$process = Start-Process -FilePath $Executable -ArgumentList $Arguments `\n"
            "    -WindowStyle Minimized -Wait -PassThru `\n"
            "    -RedirectStandardOutput $StandardOutput `\n"
            "    -RedirectStandardError $StandardError\n"
            "exit $process.ExitCode\n",
            encoding="utf-8",
        )
        launcher_result = subprocess.run(
            [
                "powershell.exe",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(launcher),
                "-Executable",
                command[0],
                "-Arguments",
                subprocess.list2cmdline(command[1:]),
                "-StandardOutput",
                str(standard_output),
                "-StandardError",
                str(standard_error),
            ],
            cwd=PROJECT_ROOT,
            capture_output=True,
            text=True,
        )
        stdout = (
            standard_output.read_text(encoding="utf-8", errors="replace")
            if standard_output.is_file()
            else ""
        )
        stderr = (
            standard_error.read_text(encoding="utf-8", errors="replace")
            if standard_error.is_file()
            else ""
        )
        if launcher_result.returncode != 0 and not stderr:
            stderr = launcher_result.stderr.strip() or launcher_result.stdout.strip()
        return subprocess.CompletedProcess(
            command,
            launcher_result.returncode,
            stdout,
            stderr,
        )


def sha256_of(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def render_case(
    executable: Path,
    case: dict,
    defaults: dict,
    output_dir: Path,
    extra_args: list[str],
) -> Path:
    """Renders one case into output_dir and returns the captured frame path."""
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.parent.mkdir(parents=True, exist_ok=True)

    command = [
        str(executable),
        "--asset", str(PROJECT_ROOT / defaults["asset"]),
        "--qa-isolation", case["isolation"],
        "--qa-camera", case["qaCamera"],
        "--qa-light", case.get("qaLight", defaults["qaLight"]),
        "--width", str(case.get("width", defaults["width"])),
        "--height", str(case.get("height", defaults["height"])),
        "--capture-dir", str(output_dir),
        "--capture-frames", str(defaults["captureFrames"]),
        "--capture-fps", str(defaults["captureFps"]),
        *extra_args,
    ]
    completed = run_renderer(command)
    if completed.returncode != 0:
        raise RuntimeError(
            f"render failed for {case['name']} (exit {completed.returncode}): "
            f"{completed.stderr.strip() or completed.stdout.strip()}"
        )
    frame = output_dir / "frame_000000.png"
    if not frame.is_file():
        raise RuntimeError(f"render produced no frame for {case['name']}")
    return frame


def compare(
    baseline: Path,
    candidate: Path,
    limits: dict,
    diff_output: Optional[Path],
) -> dict:
    reference = np.asarray(
        Image.open(baseline).convert("RGB"), dtype=np.float32) / 255.0
    actual = np.asarray(
        Image.open(candidate).convert("RGB"), dtype=np.float32) / 255.0
    if reference.shape != actual.shape:
        return {
            "status": "failed",
            "reason": "dimension-mismatch",
            "baselineShape": list(reference.shape),
            "candidateShape": list(actual.shape),
        }

    difference = np.abs(reference - actual)
    mean_error = float(difference.mean())
    changed_ratio = float(
        (difference.max(axis=2) > limits["pixelThreshold"]).mean())
    per_channel = [float(difference[:, :, c].mean()) for c in range(3)]
    passed = (
        mean_error <= limits["maxMeanError"]
        and changed_ratio <= limits["maxChangedRatio"]
    )

    if not passed and diff_output is not None:
        amplified = np.clip(difference * 8.0, 0.0, 1.0)
        diff_output.parent.mkdir(parents=True, exist_ok=True)
        Image.fromarray((amplified * 255.0).astype(np.uint8)).save(diff_output)

    return {
        "status": "passed" if passed else "failed",
        "meanAbsoluteRgbError": mean_error,
        "changedPixelRatio": changed_ratio,
        "perChannelMeanError": {
            "r": per_channel[0],
            "g": per_channel[1],
            "b": per_channel[2],
        },
        "limits": limits,
        "diffImage": str(diff_output) if not passed and diff_output else None,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--cases", type=Path, default=DEFAULT_CASES)
    parser.add_argument(
        "--baseline-dir", type=Path, default=DEFAULT_BASELINE_DIR)
    parser.add_argument(
        "--output-dir", type=Path,
        default=PROJECT_ROOT / "captures" / "visual-regression")
    parser.add_argument(
        "--tolerance", choices=["strict", "relaxed"], default="strict")
    parser.add_argument("--ci-only", action="store_true")
    parser.add_argument("--update-baseline", action="store_true")
    parser.add_argument("--summary", type=Path)
    parser.add_argument(
        "--renderer-arg",
        action="append",
        default=[],
        help="extra argument forwarded to the renderer, repeatable",
    )
    args = parser.parse_args()

    if not args.executable.is_file():
        print(f"executable not found: {args.executable}", file=sys.stderr)
        return 2

    catalog = json.loads(args.cases.read_text(encoding="utf-8"))
    defaults = catalog["defaults"]
    limits = catalog["tolerances"][args.tolerance]
    cases = [
        case for case in catalog["cases"]
        if not args.ci_only or case.get("ciEnabled", False)
    ]

    results = []
    failures = 0
    for case in cases:
        name = case["name"]
        work_dir = args.output_dir / name
        try:
            frame = render_case(
                args.executable, case, defaults, work_dir, args.renderer_arg)
        except RuntimeError as error:
            print(f"[FAIL] {name}: {error}")
            results.append(
                {"name": name, "status": "failed", "reason": str(error)})
            failures += 1
            continue

        baseline = args.baseline_dir / f"{name}.png"
        if args.update_baseline:
            baseline.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(frame, baseline)
            digest = sha256_of(baseline)
            print(f"[BASE] {name}: written sha256={digest[:16]}")
            results.append(
                {"name": name, "status": "baseline-written", "sha256": digest})
            continue

        if not baseline.is_file():
            print(f"[FAIL] {name}: baseline missing at {baseline}")
            results.append(
                {"name": name, "status": "failed",
                 "reason": "baseline-missing"})
            failures += 1
            continue

        outcome = compare(
            baseline, frame, limits, work_dir / f"{name}_diff.png")
        outcome["name"] = name
        results.append(outcome)
        if outcome["status"] == "passed":
            print(f"[ OK ] {name}: mean={outcome['meanAbsoluteRgbError']:.6f} "
                  f"changed={outcome['changedPixelRatio']:.6f}")
        else:
            failures += 1
            detail = outcome.get("reason") or (
                f"mean={outcome['meanAbsoluteRgbError']:.6f} "
                f"changed={outcome['changedPixelRatio']:.6f}")
            print(f"[FAIL] {name}: {detail}")

    summary = {
        "schemaVersion": 1,
        "tolerance": args.tolerance,
        "caseCount": len(cases),
        "failureCount": failures,
        "results": results,
    }
    encoded = json.dumps(summary, indent=2) + "\n"
    if args.summary:
        args.summary.parent.mkdir(parents=True, exist_ok=True)
        args.summary.write_text(encoded, encoding="utf-8")
    print(f"cases={len(cases)} failures={failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
