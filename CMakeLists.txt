# Copyright (c) 2026 Martial Systems LLC. All rights reserved.
#
# Headless test harness for SLEWFOLD (the instrument) and, later, SLEWFOLD FX.
# Does not build the iPlug2 plugin, does not fetch the VST3 SDK, and does not
# touch the user's plugin or settings folders. Plugin builds stay:
#   cmake -S SergeMiddleCombined -B build/SergeMiddleCombined ...
#
#   cmake --preset linux-dev
#   cmake --build --preset linux-dev
#   ctest --test-dir build/linux-dev -L fast --output-on-failure

cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SlewfoldCompilerCache.cmake")

project(SlewfoldTests VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# ---------------------------------------------------------------------------
# Settings / presets isolation
# Every CTest entry runs with HOME, XDG_*, and Windows profile variables
# pointed at a folder under the build tree. Tests must never write to
# ~/Library/Application Support/SergeMiddleCombined or any real user config.
# ---------------------------------------------------------------------------
set(SLEWFOLD_TEST_HOME "${CMAKE_BINARY_DIR}/test-home")
set(SLEWFOLD_TEST_TMP "${CMAKE_BINARY_DIR}/test-tmp")
file(MAKE_DIRECTORY
  "${SLEWFOLD_TEST_HOME}"
  "${SLEWFOLD_TEST_HOME}/.config"
  "${SLEWFOLD_TEST_HOME}/.cache"
  "${SLEWFOLD_TEST_HOME}/.local/share"
  "${SLEWFOLD_TEST_HOME}/AppData/Roaming"
  "${SLEWFOLD_TEST_HOME}/AppData/Local"
  "${SLEWFOLD_TEST_HOME}/Library/Application Support"
  "${SLEWFOLD_TEST_TMP}")
# MPLCONFIGDIR keeps matplotlib out of the real user config when the
# playground (render tier) runs under CTest.
set(SLEWFOLD_TEST_ENV
  "SLEWFOLD_TEST_HOME=${SLEWFOLD_TEST_HOME}"
  "HOME=${SLEWFOLD_TEST_HOME}"
  "XDG_CONFIG_HOME=${SLEWFOLD_TEST_HOME}/.config"
  "XDG_CACHE_HOME=${SLEWFOLD_TEST_HOME}/.cache"
  "XDG_DATA_HOME=${SLEWFOLD_TEST_HOME}/.local/share"
  "USERPROFILE=${SLEWFOLD_TEST_HOME}"
  "APPDATA=${SLEWFOLD_TEST_HOME}/AppData/Roaming"
  "LOCALAPPDATA=${SLEWFOLD_TEST_HOME}/AppData/Local"
  "TMPDIR=${SLEWFOLD_TEST_TMP}"
  "TEMP=${SLEWFOLD_TEST_TMP}"
  "TMP=${SLEWFOLD_TEST_TMP}"
  "MPLCONFIGDIR=${SLEWFOLD_TEST_HOME}/.config/matplotlib"
  "MPLBACKEND=Agg")

if(MSVC)
  set(SLEWFOLD_WARN /W4)
else()
  set(SLEWFOLD_WARN -Wall -Wextra -Wpedantic)
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# HOME is redirected for every test, which would hide packages in the
# real user site-packages (matplotlib on a --user install). Capture that
# path now, while configure still sees the real HOME, and put it on
# PYTHONPATH. CI images that install into the interpreter prefix do not
# need this.
execute_process(
  COMMAND ${Python3_EXECUTABLE} -c "import site; p = site.getusersitepackages(); print(p if p else '')"
  OUTPUT_VARIABLE SLEWFOLD_USER_SITE
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(SLEWFOLD_USER_SITE)
  list(APPEND SLEWFOLD_TEST_ENV "PYTHONPATH=${SLEWFOLD_USER_SITE}")
  message(STATUS "Test PYTHONPATH user site: ${SLEWFOLD_USER_SITE}")
endif()

enable_testing()

# Labels (ctest -L <label>):
#   fast        unit and math checks, each meant to finish in a few seconds
#   render      short offline renders (the oscilloscope playground today)
#   slow        heavy tests; they share RESOURCE_LOCK slewfold_heavy
#   instrument  the SLEWFOLD instrument suite (everything shipping today)
#   fx          reserved for the separate SLEWFOLD FX target (see hook below)
#
# Timeouts follow the jidai / rare-snail-yog tiers.

function(slewfold_apply_test_props name)
  set(options)
  set(oneValueArgs TIMEOUT LOCK)
  set(multiValueArgs LABELS ENV)
  cmake_parse_arguments(P "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  if(NOT P_TIMEOUT)
    set(P_TIMEOUT 60)
  endif()
  set(_env ${SLEWFOLD_TEST_ENV} ${P_ENV})
  set_tests_properties(${name} PROPERTIES
    LABELS "${P_LABELS}"
    TIMEOUT ${P_TIMEOUT}
    ENVIRONMENT "${_env}")
  if(P_LOCK)
    set_tests_properties(${name} PROPERTIES RESOURCE_LOCK "${P_LOCK}")
  endif()
endfunction()

function(slewfold_add_dsp_test name source)
  add_executable(${name} ${source})
  target_compile_definitions(${name} PRIVATE SERGE_REPO_ROOT="${CMAKE_SOURCE_DIR}")
  target_compile_options(${name} PRIVATE ${SLEWFOLD_WARN})
  add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${SLEWFOLD_TEST_TMP})
  slewfold_apply_test_props(${name} ${ARGN})
endfunction()

function(slewfold_add_python_test name module)
  add_test(NAME ${name}
    COMMAND ${Python3_EXECUTABLE} -m unittest ${module}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  slewfold_apply_test_props(${name} ${ARGN})
endfunction()

# ---- C++ DSP (no iPlug2) --------------------------------------------------
slewfold_add_dsp_test(test_wave_middle SergeMiddle/dsp/tests/test_wave_middle.cpp
  LABELS "fast;instrument" TIMEOUT 60)
slewfold_add_dsp_test(test_patch SergeMiddleCombined/dsp/tests/test_patch.cpp
  LABELS "fast;instrument" TIMEOUT 60)
slewfold_add_dsp_test(test_ends SergeMiddleCombined/dsp/tests/test_ends.cpp
  LABELS "fast;instrument" TIMEOUT 60)
slewfold_add_dsp_test(test_second SlopeAC/dsp/tests/test_second.cpp
  LABELS "fast;instrument" TIMEOUT 60)

# Cable geometry / Lift rope. Header-only, no iPlug2.
slewfold_add_dsp_test(test_cable_ends tests/cable_ends_main.cpp
  LABELS "fast;instrument" TIMEOUT 60)
slewfold_add_dsp_test(test_cable_rope tests/cable_rope_main.cpp
  LABELS "fast;instrument" TIMEOUT 60)

# ---- Python contract and golden locks -------------------------------------
# tests/transfer_g1.csv and tests/transfer_g1.svg are locked references.
# test_transfer.py compares the CSV to wave_middle.py within 1e-12 V (libm).
# Do not regenerate those goldens as part of this harness.
slewfold_add_python_test(py_transfer tests.test_transfer
  LABELS "fast;instrument" TIMEOUT 60)
slewfold_add_python_test(py_signal tests.test_signal
  LABELS "fast;instrument" TIMEOUT 60)
slewfold_add_python_test(py_symmetry tests.test_symmetry
  LABELS "fast;instrument" TIMEOUT 60)
slewfold_add_python_test(py_panel_layout tests.test_panel_layout
  LABELS "fast;instrument" TIMEOUT 60)
slewfold_add_python_test(py_pleat_editor tests.test_pleat_editor
  LABELS "fast;instrument" TIMEOUT 60)
slewfold_add_python_test(py_slope_editor tests.test_slope_editor
  LABELS "fast;instrument" TIMEOUT 60)
slewfold_add_python_test(py_combined_editor tests.test_combined_editor
  LABELS "fast;instrument" TIMEOUT 60)
slewfold_add_python_test(py_settings_isolation tests.test_settings_isolation
  LABELS "fast;instrument" TIMEOUT 30)
slewfold_add_python_test(py_slope slope.tests.test_slope
  LABELS "fast;instrument" TIMEOUT 60)
slewfold_add_python_test(py_slope_second slope.tests.test_second
  LABELS "fast;instrument" TIMEOUT 60)

# Prefer the CMake-built cable binaries when this suite runs under CTest, so
# the Python tests do not shell out to a hardcoded g++ (which breaks on MSVC).
# unittest discover without these variables still compiles a fallback binary.
slewfold_add_python_test(py_cable_ends tests.test_cable_ends
  LABELS "fast;instrument" TIMEOUT 60
  ENV
    "SLEWFOLD_CABLE_ENDS_BIN=${CMAKE_BINARY_DIR}/test_cable_ends${CMAKE_EXECUTABLE_SUFFIX}"
    "SLEWFOLD_CABLE_ROPE_BIN=${CMAKE_BINARY_DIR}/test_cable_rope${CMAKE_EXECUTABLE_SUFFIX}")

# ---- Oscilloscope playground (render) -------------------------------------
# tools/playground/run.py must exit 0. It builds the headless renderer if
# needed, writes plots under tools/playground/out/ (gitignored), and compares
# golden traces. Do not pass --update-golden here.
add_executable(playground tools/playground/src/playground.cpp)
target_include_directories(playground PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_options(playground PRIVATE ${SLEWFOLD_WARN})

add_test(NAME playground
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/playground/run.py"
          --build-dir "${CMAKE_BINARY_DIR}"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
slewfold_apply_test_props(playground LABELS "render;instrument" TIMEOUT 300)

# ---------------------------------------------------------------------------
# TODO(SLEWFOLD FX): when the separate effect target lands in this repo, hook
# its headless tests with one line in SLEWFOLD_FX_TESTS (and add the
# executable or Python module above, the same way as the instrument tests).
# Label them "fx" in addition to fast/render/slow so:
#   ctest -L fx           # only the effect
#   ctest -L instrument   # only the instrument (this suite)
#   ctest -L fast         # both, once FX tests exist
# Example:
#   slewfold_add_dsp_test(test_slewfold_fx Path/To/test_fx.cpp
#     LABELS "fast;fx" TIMEOUT 60)
#   list(APPEND SLEWFOLD_FX_TESTS test_slewfold_fx)
# Leave the instrument unique IDs ('Mrsy' / 'SmCb') and the FX unique ID
# (not yet assigned) untouched.
# ---------------------------------------------------------------------------
set(SLEWFOLD_FX_TESTS "")
foreach(name ${SLEWFOLD_FX_TESTS})
  set_property(TEST ${name} APPEND PROPERTY LABELS fx)
endforeach()
