"""Settings and preset tests must use a temp folder, never the real user config.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The CTest harness points HOME, XDG_*, USERPROFILE, APPDATA, and LOCALAPPDATA
at build/<preset>/test-home. A test that writes presets or iPlug2 settings
must honour that, or use tempfile, and must never open the real
Application Support / AppData folders for SergeMiddleCombined, SergeMiddle,
SlopeAC, or SLEWFOLD.
"""

from __future__ import annotations

import os
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Real user-config locations. Mentioning one of these as a path a test would
# open is forbidden in test sources (this file's token list is the exception).
_FORBIDDEN_FRAGMENTS = (
    "Library/Application Support/SergeMiddleCombined",
    "Library/Application Support/SergeMiddle",
    "Library/Application Support/SlopeAC",
    "Library/Application Support/SLEWFOLD",
    "Application Support/SergeMiddleCombined",
    "Application Support/SergeMiddle/",
)

_TEST_ROOTS = (
    ROOT / "tests",
    ROOT / "slope" / "tests",
    ROOT / "SergeMiddle" / "dsp" / "tests",
    ROOT / "SergeMiddleCombined" / "dsp" / "tests",
    ROOT / "SlopeAC" / "dsp" / "tests",
    ROOT / "tools" / "playground",
)

_SKIP_NAMES = {"test_settings_isolation.py"}


def _iter_test_sources():
    for folder in _TEST_ROOTS:
        if not folder.is_dir():
            continue
        for path in folder.rglob("*"):
            if path.suffix.lower() not in {".py", ".cpp", ".h", ".hpp", ".cc", ".c", ".mm"}:
                continue
            if path.name in _SKIP_NAMES:
                continue
            if "golden" in path.parts or "out" in path.parts:
                continue
            yield path


class SettingsIsolationTest(unittest.TestCase):
    def test_no_test_source_hardcodes_real_user_config(self) -> None:
        hits: list[str] = []
        for path in _iter_test_sources():
            text = path.read_text(encoding="utf-8", errors="replace")
            for fragment in _FORBIDDEN_FRAGMENTS:
                if fragment in text:
                    rel = path.relative_to(ROOT)
                    hits.append(f"{rel}: {fragment}")
        self.assertEqual(
            hits,
            [],
            "test sources must not hard-code the real user config path; "
            "use tempfile or the CTest HOME redirect (build/*/test-home)",
        )

    def test_ctest_home_is_not_the_real_user_home(self) -> None:
        # When this test runs under CTest, HOME (and USERPROFILE) point at
        # the build-tree sandbox. unittest discover without CTest leaves
        # the real environment alone; we only assert the redirect when set.
        sandbox = os.environ.get("SLEWFOLD_TEST_HOME")
        home = os.environ.get("HOME") or os.environ.get("USERPROFILE") or ""
        if not sandbox:
            # CTest also sets HOME to build/*/test-home. If that directory
            # name appears, treat it as redirected even without the helper var.
            if "test-home" in Path(home).parts:
                return
            self.skipTest("not running under the CTest HOME redirect")
        self.assertEqual(Path(home).resolve(), Path(sandbox).resolve())
        real_mac = Path.home()
        # Path.home() follows HOME, so compare against the sandbox instead
        # of claiming we can see the original home. The sandbox must live
        # under a temp or build folder, not under Application Support.
        self.assertNotIn("Application Support", str(Path(home)))
        self.assertTrue(
            "test-home" in Path(home).parts or Path(home).is_relative_to(Path(tempfile.gettempdir())),
            f"HOME {home} is not a test sandbox",
        )
        _ = real_mac


if __name__ == "__main__":
    unittest.main()
