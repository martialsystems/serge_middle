# Fast tests and lean CI

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

CTest labels split the suite so a save-and-rebuild loop stays short. `ctest -L fast` is the default loop; `render` and `slow` are for longer checks before you call a change done. This harness builds only the headless DSP tests and the oscilloscope playground. It does not fetch iPlug2 or the VST3 SDK, and it does not write to the user's plugin or settings folders.

| Label | What it is | How to run |
|---|---|---|
| `fast` | Python contract tests (including the locked `tests/transfer_g1.csv` curve, compared within 1e-12 V so a libm difference of a few ULPs still passes), C++ DSP tests, and the cable/rope checks. Each test is meant to finish in a few seconds. | `ctest --test-dir build/<preset> -L fast --output-on-failure` |
| `render` | The oscilloscope playground, `tools/playground/run.py`. It must exit 0. | `ctest --test-dir build/<preset> -L render` |
| `slow` | Heavy tests. They share a `RESOURCE_LOCK` (`slewfold_heavy`) so only one runs at a time, even with `ctest -j`. No slow tests ship today; the lock is in place for later work. | `ctest --test-dir build/<preset> -L slow` |

Current tests also carry the `instrument` label (the SLEWFOLD instrument). The `fx` label is reserved for the separate SLEWFOLD FX target.

`tests/transfer_g1.csv`, `tests/transfer_g1.svg`, and the playground / slope golden CSVs are locked references. Do not regenerate them as part of the test run.

## Presets and the compiler cache

CMake presets live in `CMakePresets.json` (CMake 3.22). From the repository root, with Ninja:

```sh
cmake --preset linux-dev    # mac-dev or windows-dev on those machines
cmake --build --preset linux-dev
ctest --test-dir build/linux-dev -L fast --output-on-failure
# or, with the linux-ci preset configured: ctest --preset fast
```

Windows presets assume a Visual Studio Developer environment (or `cl` and Ninja already on `PATH`). The macOS `mac-release` preset matches the release workflow's universal architecture flags (`arm64;x86_64`, deployment target 11.0). That preset configures the *test* tree; the plugin itself is still `cmake -S SergeMiddleCombined ...` as in `BUILD.md`.

`SLEWFOLD_USE_CCACHE` is on by default. If `sccache` or `ccache` is on `PATH` and the generator is Ninja or Makefiles, CMake uses it as the compiler launcher. Pass `-DSLEWFOLD_USE_CCACHE=OFF`, or set `CMAKE_CXX_COMPILER_LAUNCHER` yourself, to skip that.

The older entry points still work: `python3 -m unittest discover -s tests -t .`, `python3 -m unittest discover -s slope/tests -t .`, `bash SergeMiddleCombined/dsp/tests/run_tests.sh`, `bash SlopeAC/dsp/tests/run_tests.sh`, and `python3 tools/playground/run.py`.

## Settings and presets

Every CTest entry runs with `HOME`, `XDG_*`, `USERPROFILE`, `APPDATA`, and `LOCALAPPDATA` pointed at `build/<preset>/test-home`. A test that reads or writes presets or iPlug2 settings must use that sandbox, or a `tempfile` folder, and must never open the real `~/Library/Application Support/SergeMiddleCombined` (or the equivalent AppData path). `tests/test_settings_isolation.py` greps the test sources for those paths.

## Watch scripts

`tools/test_watch.sh` (macOS/Linux) and `tools/test_watch.ps1` (Windows) configure if needed, then rebuild and run `ctest -L fast` whenever sources change.

```sh
tools/test_watch.sh --preset linux-dev
tools/test_watch.sh --once --jobs 2
```

```powershell
tools/test_watch.ps1 -Preset windows-dev
tools/test_watch.ps1 -Once -Jobs 2
```

The shell script uses `watchexec` when it is installed, then `entr`, and otherwise polls every 2 s. No extra package is required.

## CI

Pull requests and pushes to `main` run an Ubuntu-only workflow (Ninja, ccache, `ctest -L fast` then `-L render`). Path filters skip the job when the change is outside the build and test inputs. macOS and Windows plugin builds stay on the Release workflow, which runs when you push a `v*` tag and attaches `SLEWFOLD-macOS.zip` and `SLEWFOLD-Windows.zip` to a draft release. The previous `playground.yml` (Ubuntu + macOS on every push and PR) is removed; the playground is the render tier on Ubuntu.

## SLEWFOLD FX hook

When the separate effect target is added to this repository, hook its headless tests with one line in `SLEWFOLD_FX_TESTS` inside the root `CMakeLists.txt` (and add the executable or Python module the same way as the instrument tests). Label them `fx` in addition to `fast` / `render` / `slow`:

```sh
ctest -L fx           # only the effect, once tests exist
ctest -L instrument   # only the instrument (this suite)
ctest -L fast         # both
```

Keep `PLUG_MFR_ID` `'Mrsy'`. Do not reuse the instrument `PLUG_UNIQUE_ID` `'SmCb'`.

## Measured on the Linux box (21 Nov 2026, America/Chicago)

Conditions: GCC 14.2, Ninja, ccache, RelWithDebInfo (`linux-dev`), `-j2`, Python 3.13.5. numpy 2.5.5 and matplotlib 3.11.2 were already installed in the user site-packages. Load was otherwise quiet.

| Step | Wall time |
|---|---|
| Configure (`linux-dev`, ccache detected) | 0.53 s |
| Build test targets (cold) | 5.15 s |
| Incremental rebuild (no source change) | ninja: no work to do |
| `ctest -L fast -j2` | 2.95 s, 17/17 passed (5.90 s CPU). Longest: `test_patch` 2.89 s, `test_wave_middle` 1.34 s, `py_transfer` 0.68 s |
| Full suite (`ctest -j2`, fast + render) | 27.20 s, 18/18 passed |
| `ctest -L render` (`playground`) | 27.19 s, exit 0 |
| `tools/test_watch.sh --once --jobs 2` (warm) | 3.0 s, 17/17 passed (build 0 s) |

The slow tier has no tests yet (`ctest -L slow` reports 0 tests). Windows and macOS presets and watchers were not executed on those OSes. GitHub Actions cache hit rates are unknown until this branch's Ubuntu workflow has run.
