#!/usr/bin/env python3
# Render every playground patch, draw the scopes, and run the spec checks.
# Copyright (c) 2026 Martial Systems LLC. All rights reserved.

"""Headless runner for the Serge Middle patch playground.

Builds tools/playground, renders each patch in patches/ through the C++ DSP,
writes a PNG scope and an HTML index, and compares golden traces.
"""

from __future__ import annotations

import argparse
import csv
import html
import json
import math
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
PATCH_DIR = HERE / "patches"
GOLDEN_DIR = HERE / "golden"
OUT_DIR = HERE / "out"

# Absolute volts. Large enough for the libm gaps measured on the locked
# curves (1–2 ULP on the sine envelope, 12 ULP on one transfer sample) and
# small enough that a one-sample slope error, about 20 mV, still fails.
DEFAULT_GOLDEN_TOL = 1e-5


class CheckError(Exception):
    pass


def build_cli(build_dir: Path) -> Path:
    exe = build_dir / "playground"
    cache = build_dir / "CMakeCache.txt"
    if not cache.exists():
        cmd = [
            "cmake",
            "-S",
            str(HERE),
            "-B",
            str(build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
        ]
        if sys.platform.startswith("linux") and not os.environ.get("CXX"):
            cmd.append("-DCMAKE_CXX_COMPILER=g++")
        subprocess.check_call(cmd)
    # Incremental. A DSP header change rebuilds even when playground.cpp did not.
    subprocess.check_call(["cmake", "--build", str(build_dir), "-j"])
    if exe.exists():
        return exe
    for candidate in (build_dir / "Release" / "playground", build_dir / "Release" / "playground.exe"):
        if candidate.exists():
            return candidate
    raise SystemExit(f"playground binary not found in {build_dir}")


def read_csv(path: Path) -> dict[str, list[float]]:
    with path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        raise CheckError(f"{path} is empty")
    cols: dict[str, list[float]] = {key: [] for key in rows[0]}
    for row in rows:
        for key, value in row.items():
            cols[key].append(float(value))
    return cols


def window(check: dict, n: int, fs: float) -> tuple[int, int]:
    if "t0_sample" in check:
        return int(check["t0_sample"]), int(check["t1_sample"])
    i0 = int(round(float(check.get("t0_ms", 0)) * fs / 1000.0))
    if "t1_ms" in check:
        i1 = int(round(float(check["t1_ms"]) * fs / 1000.0))
    elif "skip_ms" in check:
        i0 = int(round(float(check["skip_ms"]) * fs / 1000.0))
        i1 = n
    else:
        i1 = n
    i0 = max(0, min(n, i0))
    i1 = max(i0, min(n, i1))
    return i0, i1


def column(cols: dict[str, list[float]], name: str) -> list[float]:
    if name not in cols:
        raise CheckError(f"jack {name} is not in this render")
    return cols[name]


def expect_samples(check: dict, fs: float) -> int:
    samples = check["samples"]
    if isinstance(samples, dict):
        key = str(int(fs))
        if key not in samples:
            raise CheckError(f"no expected period for {key} Hz")
        return int(samples[key])
    return int(samples)


def rise_starts(values: list[float], i0: int, i1: int, tol: float = 1e-8) -> list[int]:
    starts = []
    last = min(i1, len(values)) - 1
    for i in range(i0, last):
        if abs(values[i]) <= tol and values[i + 1] > tol:
            starts.append(i)
    return starts


def check_hold(cols, check, fs):
    values = column(cols, check["jack"])
    i0, i1 = window(check, len(values), fs)
    if i1 <= i0:
        raise CheckError("empty window")
    tol = float(check.get("tol", 1e-9))
    volts = float(check["volts"])
    worst = 0.0
    at = i0
    for i in range(i0, i1):
        err = abs(values[i] - volts)
        if err > worst:
            worst = err
            at = i
    if worst > tol:
        raise CheckError(f"sample {at} is {values[at]:.6g} V, want {volts:g} (err {worst:.3g})")
    return f"{volts:g} V over samples {i0}..{i1}"


def check_oneshot(cols, check, fs):
    del fs
    values = column(cols, check["jack"])
    rise = int(check["rise_samples"])
    hold = int(check["hold_last"])
    fall = int(check["fall_samples"])
    tol = float(check.get("tol", 1e-9))
    end = hold + fall
    if end >= len(values):
        raise CheckError(f"render ends at sample {len(values) - 1}, oneshot needs {end}")
    span = 5.0

    def near(i: int, want: float) -> None:
        if abs(values[i] - want) > tol:
            raise CheckError(f"sample {i} is {values[i]:.9g} V, want {want:.9g}")

    for i in range(rise):
        near(i, span * i / rise)
    for i in range(rise, hold + 1):
        near(i, span)
    for j in range(fall):
        near(hold + j, span * (fall - j) / fall)
    near(end, 0.0)
    return f"rise {rise}, hold through {hold}, fall {fall}, back to 0 at {end}"


def check_period(cols, check, fs):
    values = column(cols, check["jack"])
    i0, i1 = window(check, len(values), fs)
    want = expect_samples(check, fs)
    starts = rise_starts(values, i0, i1)
    gaps = [b - a for a, b in zip(starts, starts[1:])]
    if not gaps:
        raise CheckError(f"fewer than two rise starts in samples {i0}..{i1} (starts {starts})")
    bad = [g for g in gaps if g != want]
    if bad:
        raise CheckError(f"gaps {gaps[:6]} , want {want}")
    hz = fs / want
    return f"{want} samples, {hz:.6f} Hz, {len(gaps)} cycles"


def check_peak_offset(cols, check, fs):
    values = column(cols, check["jack"])
    i0, i1 = window(check, len(values), fs)
    volts = float(check.get("volts", 5.0))
    tol = float(check.get("tol", 1e-9))
    offset = int(check["offset"])
    for i in range(i0, i1):
        if abs(values[i] - volts) <= tol:
            if i - i0 != offset:
                raise CheckError(f"first {volts:g} V at offset {i - i0}, want {offset}")
            return f"offset {i - i0} samples (absolute sample {i})"
    raise CheckError(f"never reached {volts:g} V")


def check_peak_abs(cols, check, fs):
    values = column(cols, check["jack"])
    i0, i1 = window(check, len(values), fs)
    if i1 <= i0:
        raise CheckError("empty window")
    peak = max(abs(v) for v in values[i0:i1])
    lo = float(check["min"])
    hi = float(check["max"])
    if peak < lo or peak > hi:
        raise CheckError(f"peak |{check['jack']}| is {peak:.6g} V, want {lo:g}..{hi:g}")
    return f"peak {peak:.6g} V"


def check_mean_abs(cols, check, fs):
    values = column(cols, check["jack"])
    i0, i1 = window(check, len(values), fs)
    if i1 <= i0:
        raise CheckError("empty window")
    mean = abs(sum(values[i0:i1]) / (i1 - i0))
    limit = float(check["max"])
    if mean > limit:
        raise CheckError(f"|mean| is {mean:.6g} V, limit {limit:g}")
    return f"|mean| {mean:.3g} V"


def check_silent(cols, check, fs):
    values = column(cols, check["jack"])
    i0, i1 = window(check, len(values), fs)
    peak = max(abs(v) for v in values[i0:i1])
    limit = float(check.get("max_abs", 1e-9))
    if peak > limit:
        raise CheckError(f"peak {peak:.6g} V, silent limit {limit:g}")
    return f"peak {peak:.3g} V"


def check_not_silent(cols, check, fs):
    values = column(cols, check["jack"])
    i0, i1 = window(check, len(values), fs)
    peak = max(abs(v) for v in values[i0:i1])
    floor = float(check.get("min_abs", 0.05))
    if peak < floor:
        raise CheckError(f"peak {peak:.6g} V, want at least {floor:g}")
    return f"peak {peak:.6g} V"


def check_asymmetry(cols, check, fs):
    values = column(cols, check["jack"])
    i0, i1 = window(check, len(values), fs)
    segment = values[i0:i1]
    total = max(segment) + min(segment)
    mag = abs(total)
    if "max_sum" in check and mag > float(check["max_sum"]):
        raise CheckError(f"|max+min| is {mag:.6g} V, want <= {check['max_sum']}")
    if "min_sum" in check and mag < float(check["min_sum"]):
        raise CheckError(f"|max+min| is {mag:.6g} V, want >= {check['min_sum']}")
    return f"|max+min| {mag:.6g} V"


def count_peaks(values: list[float], i0: int, i1: int, thresh: float) -> int:
    count = 0
    last = min(i1, len(values) - 1)
    for i in range(max(i0, 1), last):
        if values[i] > values[i - 1] and values[i] >= values[i + 1] and values[i] > thresh:
            count += 1
    return count


def check_more_peaks(cols, check, fs):
    values = column(cols, check["jack"])
    n = len(values)
    early = check["early"]
    late = check["late"]
    e0, e1 = int(round(early[0] * fs / 1000.0)), int(round(early[1] * fs / 1000.0))
    l0, l1 = int(round(late[0] * fs / 1000.0)), int(round(late[1] * fs / 1000.0))
    thresh = float(check.get("thresh", 0.05))
    before = count_peaks(values, e0, min(e1, n), thresh)
    after = count_peaks(values, l0, min(l1, n), thresh)
    extra = int(check.get("min_extra", 2))
    if after < before + extra:
        raise CheckError(f"late peaks {after}, early peaks {before}, want at least {extra} more")
    return f"peaks {before} then {after}"


def check_jacks_differ(cols, check, fs):
    left = column(cols, check["jack"])
    right = column(cols, check["other_jack"])
    i0, i1 = window(check, len(left), fs)
    if i1 <= i0:
        raise CheckError("empty window")
    if i1 > len(right):
        raise CheckError("other jack is shorter than this window")
    rms = math.sqrt(sum((a - b) ** 2 for a, b in zip(left[i0:i1], right[i0:i1])) / (i1 - i0))
    floor = float(check.get("min_rms", 0.02))
    if rms < floor:
        raise CheckError(f"rms difference {rms:.6g} V, want at least {floor:g}")
    return f"rms difference {rms:.6g} V"


def check_windows_differ(cols, check, fs):
    values = column(cols, check["jack"])
    a0, a1 = [int(round(ms * fs / 1000.0)) for ms in check["a_ms"]]
    b0, b1 = [int(round(ms * fs / 1000.0)) for ms in check["b_ms"]]
    left = values[a0:a1]
    right = values[b0:b1]
    if len(left) != len(right) or not left:
        raise CheckError(f"windows have lengths {len(left)} and {len(right)}")
    rms = math.sqrt(sum((x - y) ** 2 for x, y in zip(left, right)) / len(left))
    floor = float(check.get("min_rms", 0.02))
    if rms < floor:
        raise CheckError(f"rms difference {rms:.6g} V, want at least {floor:g}")
    return f"rms difference {rms:.6g} V"


def check_matches_other(cols, check, fs, renders):
    values = column(cols, check["jack"])
    i0, i1 = window(check, len(values), fs)
    other_name = check["other"]
    other_rate = int(check.get("other_rate", fs))
    key = (other_name, other_rate)
    if key not in renders:
        raise CheckError(f"missing render {other_name} at {other_rate}")
    other_cols = renders[key]
    other = column(other_cols, check["other_jack"])
    o0 = int(round(float(check.get("other_t0_ms", 0)) * other_rate / 1000.0))
    length = i1 - i0
    if o0 + length > len(other):
        raise CheckError("other render is shorter than this window")
    tol = float(check.get("tol", 1e-9))
    worst = 0.0
    at = 0
    for k in range(length):
        err = abs(values[i0 + k] - other[o0 + k])
        if err > worst:
            worst = err
            at = k
    if worst > tol:
        raise CheckError(f"differs from {other_name} by {worst:.3g} V at offset {at}")
    return f"matches {other_name} within {worst:.3g} V"


CHECKS = {
    "hold": check_hold,
    "oneshot": check_oneshot,
    "period": check_period,
    "peak_offset": check_peak_offset,
    "peak_abs": check_peak_abs,
    "mean_abs": check_mean_abs,
    "silent": check_silent,
    "not_silent": check_not_silent,
    "asymmetry": check_asymmetry,
    "more_peaks": check_more_peaks,
    "windows_differ": check_windows_differ,
    "jacks_differ": check_jacks_differ,
}


def output_jacks(engine: str, cols: dict[str, list[float]]) -> list[str]:
    if engine == "slope":
        return [name for name in ("out", "end", "ac", "ac_coupled") if name in cols]
    if engine == "pleat":
        return ["out"] if "out" in cols else []
    names = (
        "pleat_out",
        "slope_end",
        "slope_out",
        "slope_ac",
        "top_out",
        "bottom_out1",
        "bottom_out2",
        "slope2_end",
        "slope2_out",
        "slope2_ac",
    )
    return [name for name in names if name in cols]


def automatic_checks(engine: str, cols: dict[str, list[float]]) -> list[tuple[str, bool, str]]:
    """Each entry is (id, passed, detail). A failed rail does not hide the others."""
    results: list[tuple[str, bool, str]] = []
    bad = []
    for name, values in cols.items():
        if name == "sample":
            continue
        if any(not math.isfinite(v) for v in values):
            bad.append(name)
    if bad:
        results.append(("finite", False, f"{', '.join(bad)} has a non-finite sample"))
    else:
        results.append(("finite", True, "every jack is finite"))

    pairs = []
    ac = None
    if engine == "slope":
        pairs.append(("out", "end"))
        ac = cols["ac"]
    elif engine == "patch":
        pairs.append(("slope_out", "slope_end"))
        if "slope2_out" in cols and "slope2_end" in cols:
            pairs.append(("slope2_out", "slope2_end"))

    end_problem = None
    for out_name, end_name in pairs:
        for i, (level, gate) in enumerate(zip(cols[out_name], cols[end_name])):
            if abs(gate) > 1e-9 and abs(gate - 5.0) > 1e-9:
                end_problem = f"{end_name} sample {i} is {gate:.6g} V, want 0 or 5"
                break
            if abs(level) <= 1e-12 and abs(gate - 5.0) > 1e-9:
                end_problem = f"{out_name} sample {i} is 0 and END is {gate:.6g}"
                break
            if level >= 4.0 and abs(gate) > 1e-9:
                end_problem = f"{out_name} sample {i} is {level:.6g} and END is {gate:.6g}"
                break
        if end_problem:
            break
    if pairs:
        if end_problem:
            results.append(("end_law", False, end_problem))
        else:
            results.append(("end_law", True, "END is 0 V or +5 V, high at 0 V, low from 4 V up"))

    if ac is not None and "out" in cols:
        worst = max(abs(a - (2.5 - level)) for a, level in zip(ac, cols["out"]))
        if worst > 1e-9:
            results.append(("ac_identity", False, f"AC differs from 2.5 - OUT by {worst:.3g} V"))
        else:
            results.append(("ac_identity", True, "AC = 2.5 - OUT"))

    limit = 5.0 + 1e-6
    unipolar = {"out"} if engine == "slope" else {"slope_out", "slope2_out"} if engine == "patch" else set()
    problem = None
    for name in output_jacks(engine, cols):
        values = cols[name]
        if name in unipolar:
            if any(v < -1e-9 or v > limit for v in values):
                problem = f"{name} spans {min(values):.6g}..{max(values):.6g} V"
                break
        elif any(abs(v) > limit for v in values):
            problem = f"{name} peaks at {max(abs(v) for v in values):.6g} V"
            break
    if problem:
        results.append(("clamp_outputs", False, problem))
    else:
        results.append(("clamp_outputs", True, "output jacks stay within the Serge ±5 V rails"))
    return results


def compare_golden(cols: dict[str, list[float]], golden: Path, tol: float) -> str:
    if not golden.exists():
        raise CheckError(f"missing golden {golden.name}")
    ref = read_csv(golden)
    got_names = list(cols)
    ref_names = list(ref)
    if got_names[: len(ref_names)] != ref_names:
        raise CheckError(f"columns {got_names} vs {ref_names}")
    if len(cols["sample"]) != len(ref["sample"]):
        raise CheckError(f"length {len(cols['sample'])} vs {len(ref['sample'])}")
    worst = 0.0
    where = ""
    for name in ref_names:
        if name == "sample":
            continue
        for i, (got, want) in enumerate(zip(cols[name], ref[name])):
            err = abs(got - want)
            if err > worst:
                worst = err
                where = f"{name} sample {i}"
    if worst > tol:
        raise CheckError(f"{where} differs by {worst:.3g} V, tolerance {tol:g}")
    return f"max abs {worst:.3g} V, tolerance {tol:g}"


def plot_scope(path: Path, title: str, cols: dict[str, list[float]]) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    names = [name for name in cols if name not in ("sample", "time_ms")]
    time = cols["time_ms"]
    fig, axes = plt.subplots(len(names), 1, sharex=True, figsize=(10, 1.15 * len(names) + 0.6), squeeze=False)
    palette = ["#1f4b99", "#b45309", "#0f766e", "#9f1239", "#4338ca", "#3f6212", "#9a3412", "#155e75"]
    for index, name in enumerate(names):
        ax = axes[index][0]
        ax.plot(time, cols[name], color=palette[index % len(palette)], linewidth=0.8)
        ax.set_ylabel(f"{name}\n(V)", fontsize=8)
        ax.grid(True, alpha=0.35)
    axes[-1][0].set_xlabel("time (ms)")
    fig.suptitle(title, fontsize=12)
    fig.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=110)
    plt.close(fig)


def write_index(path: Path, runs: list[dict]) -> None:
    passed = sum(1 for run in runs if run["ok"])
    known = [item for run in runs for item in run["checks"] if item["status"] == "known"]
    rows = []
    for run in runs:
        status = "pass" if run["ok"] else "fail"
        known_n = sum(1 for item in run["checks"] if item["status"] == "known")
        rows.append(
            "<tr>"
            f"<td>{html.escape(run['name'])}</td>"
            f"<td>{run['rate']:.0f}</td>"
            f"<td>{status}</td>"
            f"<td>{known_n}</td>"
            f"<td><a href=\"{html.escape(run['plot_rel'])}\">scope</a></td>"
            "</tr>"
        )
    blocks = []
    for run in runs:
        items = []
        for item in run["checks"]:
            items.append(
                f"<li><b>{html.escape(item['status'])}</b> "
                f"{html.escape(item['id'])}: {html.escape(item['detail'])}</li>"
            )
        blocks.append(
            f"<h2>{html.escape(run['name'])} · {run['rate']:.0f} Hz</h2>"
            f"<img src=\"{html.escape(run['plot_rel'])}\" alt=\"{html.escape(run['name'])} scope\">"
            f"<ul>{''.join(items)}</ul>"
        )
    known_html = ""
    if known:
        lines = "".join(
            f"<li><b>{html.escape(item['id'])}</b> ({html.escape(item['run'])}): {html.escape(item['detail'])}</li>"
            for item in known
        )
        known_html = f"<h2>Known failures</h2><ul>{lines}</ul>"
    page = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Serge Middle patch playground</title>
<style>
body {{ font: 15px/1.45 sans-serif; margin: 2rem; color: #1c1917; }}
table {{ border-collapse: collapse; }}
td, th {{ border-bottom: 1px solid #d6d3d1; padding: 0.3rem 0.8rem; text-align: left; }}
img {{ max-width: 100%; height: auto; }}
</style>
</head>
<body>
<h1>Serge Middle patch playground</h1>
<p>{passed} of {len(runs)} renders passed. Spec checks that the current DSP does not meet are listed as known failures and do not fail the run.</p>
{known_html}
<table>
<tr><th>Patch</th><th>Hz</th><th>Result</th><th>Known</th><th>Plot</th></tr>
{''.join(rows)}
</table>
{''.join(blocks)}
</body>
</html>
"""
    path.write_text(page, encoding="utf-8")


def rates_of(patch: dict) -> list[float]:
    if "sample_rates" in patch:
        return [float(rate) for rate in patch["sample_rates"]]
    return [float(patch.get("sample_rate", 48000))]


def run_patch(cli: Path, patch_path: Path, patch: dict, rate: float, csv_path: Path) -> None:
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    cmd = [str(cli), "--patch", str(patch_path), "--out", str(csv_path), "--sample-rate", f"{rate:.0f}"]
    proc = subprocess.run(cmd, check=False, capture_output=True, text=True)
    if proc.returncode != 0:
        raise SystemExit(proc.stderr.strip() or proc.stdout.strip() or f"{cli} failed")


def evaluate(patch: dict, engine: str, cols, fs: float, renders, golden: Path, update: bool) -> list[dict]:
    known = patch.get("known_failures", {})
    items = []

    def record(check_id: str, status: str, detail: str) -> None:
        if status == "fail" and check_id in known:
            reason = known[check_id]
            items.append({"id": check_id, "status": "known", "detail": f"{detail} Known: {reason}"})
            return
        if status == "pass" and check_id in known:
            items.append(
                {
                    "id": check_id,
                    "status": "fail",
                    "detail": f"passed ({detail}), but it is still listed as a known failure",
                }
            )
            return
        items.append({"id": check_id, "status": status, "detail": detail})

    for check_id, ok, detail in automatic_checks(engine, cols):
        record(check_id, "pass" if ok else "fail", detail)

    for check in patch.get("checks", []):
        check_id = check["id"]
        kind = check["type"]
        try:
            if kind == "matches_other":
                detail = check_matches_other(cols, check, fs, renders)
            else:
                detail = CHECKS[kind](cols, check, fs)
            record(check_id, "pass", detail)
        except CheckError as exc:
            record(check_id, "fail", str(exc))
        except KeyError as exc:
            record(check_id, "fail", f"bad check: {exc}")

    tol = float(patch.get("golden_abs_tol", DEFAULT_GOLDEN_TOL))
    try:
        if update:
            detail = f"updated {golden.name}, tolerance {tol:g}"
            record("golden", "pass", detail)
        else:
            detail = compare_golden(cols, golden, tol)
            record("golden", "pass", detail)
    except CheckError as exc:
        record("golden", "fail", str(exc))
    return items


def main() -> int:
    parser = argparse.ArgumentParser(description="Render and check the patch playground.")
    parser.add_argument("--update-golden", action="store_true", help="Rewrite golden CSVs from this render.")
    parser.add_argument("--patch", action="append", default=[], help="Patch name to run. Default: all.")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build" / "playground")
    args = parser.parse_args()

    cli = build_cli(args.build_dir)
    paths = sorted(PATCH_DIR.glob("*.json"))
    if args.patch:
        wanted = set(args.patch)
        paths = [path for path in paths if path.stem in wanted]
        missing = wanted - {path.stem for path in paths}
        if missing:
            raise SystemExit(f"unknown patch {sorted(missing)}")
    if not paths:
        raise SystemExit(f"no patches in {PATCH_DIR}")

    plot_dir = OUT_DIR / "plots"
    csv_dir = OUT_DIR / "csv"
    plot_dir.mkdir(parents=True, exist_ok=True)
    renders: dict[tuple[str, int], dict[str, list[float]]] = {}
    jobs = []
    for path in paths:
        patch = json.loads(path.read_text(encoding="utf-8"))
        for rate in rates_of(patch):
            rate_i = int(rate)
            csv_path = csv_dir / f"{patch['name']}_{rate_i}.csv"
            run_patch(cli, path, patch, rate, csv_path)
            cols = read_csv(csv_path)
            renders[(patch["name"], rate_i)] = cols
            jobs.append((path, patch, rate, cols, csv_path))

    runs = []
    failures = 0
    for path, patch, rate, cols, csv_path in jobs:
        rate_i = int(rate)
        golden = GOLDEN_DIR / f"{patch['name']}_{rate_i}.csv"
        if args.update_golden:
            GOLDEN_DIR.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(csv_path, golden)
        items = evaluate(patch, patch["engine"], cols, rate, renders, golden, args.update_golden)
        for item in items:
            item["run"] = f"{patch['name']} {rate_i}"
        ok = all(item["status"] != "fail" for item in items)
        failures += sum(1 for item in items if item["status"] == "fail")
        plot_rel = f"plots/{patch['name']}_{rate_i}.png"
        plot_scope(plot_dir / f"{patch['name']}_{rate_i}.png", f"{patch['name']}  {rate_i} Hz", cols)
        runs.append({"name": patch["name"], "rate": rate, "ok": ok, "checks": items, "plot_rel": plot_rel})
        print(f"{'ok  ' if ok else 'FAIL'} {patch['name']} {rate_i}")
        for item in items:
            if item["status"] != "pass":
                print(f"    {item['status']:5} {item['id']}: {item['detail']}")

    write_index(OUT_DIR / "index.html", runs)
    (OUT_DIR / "results.json").write_text(json.dumps(runs, indent=2), encoding="utf-8")
    print(f"index {OUT_DIR / 'index.html'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
