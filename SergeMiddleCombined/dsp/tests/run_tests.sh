#!/bin/bash
# Build and run the combined patch DSP tests. No VST3 SDK.
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
build="$root/build/combined-dsp"
cmake -S "$root/SergeMiddleCombined/dsp/tests" -B "$build" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build"
ctest --test-dir "$build" --output-on-failure
