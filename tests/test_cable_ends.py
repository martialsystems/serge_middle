"""Default routing draws no cables, and every end is an ids_v6 jack.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def _prebuilt(env_name: str, stem: str) -> Path | None:
    override = os.environ.get(env_name)
    if override:
        path = Path(override)
        if path.is_file():
            return path
    suffix = ".exe" if os.name == "nt" else ""
    name = stem + suffix
    roots = [ROOT / "build"]
    build = ROOT / "build"
    if build.is_dir():
        roots.extend(sorted(p for p in build.iterdir() if p.is_dir()))
    for folder in roots:
        for candidate in (
            folder / name,
            folder / "Release" / name,
            folder / "Debug" / name,
        ):
            if candidate.is_file():
                return candidate
    return None


def _cxx_command(source: Path, binary: Path) -> list[str]:
    cxx = os.environ.get("CXX")
    if cxx:
        return [cxx, "-std=c++17", "-O2", "-o", str(binary), str(source)]
    if shutil.which("g++"):
        return ["g++", "-std=c++17", "-O2", "-o", str(binary), str(source)]
    if shutil.which("c++"):
        return ["c++", "-std=c++17", "-O2", "-o", str(binary), str(source)]
    if shutil.which("clang++"):
        return ["clang++", "-std=c++17", "-O2", "-o", str(binary), str(source)]
    raise FileNotFoundError("no C++ compiler on PATH (g++, c++, clang++, or $CXX)")


def _run_binary(env_name: str, stem: str, source: Path, expected: str) -> str:
    prebuilt = _prebuilt(env_name, stem)
    if prebuilt is not None:
        binary = prebuilt
    else:
        binary = ROOT / "build" / stem
        binary.parent.mkdir(parents=True, exist_ok=True)
        compiled = subprocess.run(
            _cxx_command(source, binary),
            check=False,
            capture_output=True,
            text=True,
        )
        if compiled.returncode != 0:
            raise AssertionError(compiled.stderr)
    ran = subprocess.run([str(binary)], check=False, capture_output=True, text=True)
    if ran.returncode != 0:
        raise AssertionError(ran.stderr or ran.stdout)
    if expected not in ran.stdout:
        raise AssertionError(ran.stdout)
    return ran.stdout


class CableEndsTest(unittest.TestCase):
    def test_table_matches_ids_v6(self) -> None:
        ids = json.loads((ROOT / "tools" / "art" / "ids_v6.json").read_text(encoding="utf-8"))
        centres = {row[0]: (row[1], row[2]) for row in ids}
        face = (ROOT / "tools" / "art" / "slewfold_face.py").read_text(encoding="utf-8")
        header = (ROOT / "SergeMiddleCombined" / "CableEnds.h").read_text(encoding="utf-8")
        self.assertIn("jack_pleat_in2", centres)
        self.assertEqual(centres["jack_pleat_in2"], (600, 305))
        self.assertEqual(centres["jack_pleat_out"], (1000, 305))
        self.assertEqual(centres["jack_slope1_trig"], (90, 725))
        self.assertNotIn((0, 0), centres.values())
        for name, xy in centres.items():
            if not name.startswith("jack_"):
                continue
            self.assertIn(f'"{name}"', header)
            self.assertIn(f"{xy[0]}.f, {xy[1]}.f", header)
            self.assertIn(f'"{name}"', face)

    def test_default_draws_no_cables(self) -> None:
        self.assertIn(
            "cable ends ok",
            _run_binary(
                "SLEWFOLD_CABLE_ENDS_BIN",
                "test_cable_ends",
                ROOT / "tests" / "cable_ends_main.cpp",
                "cable ends ok",
            ),
        )

    def test_rope_follows_a_moved_pin(self) -> None:
        self.assertIn(
            "cable rope ok",
            _run_binary(
                "SLEWFOLD_CABLE_ROPE_BIN",
                "test_cable_rope",
                ROOT / "tests" / "cable_rope_main.cpp",
                "cable rope ok",
            ),
        )


if __name__ == "__main__":
    unittest.main()
