#!/bin/bash
# Build and run the second-half DSP port. No VST3 SDK.
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
build="$root/build/slope-ac-dsp"
cmake -S "$root/SlopeAC/dsp/tests" -B "$build" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build"
ctest --test-dir "$build" --output-on-failure
