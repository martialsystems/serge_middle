# Standalone tests for the DSP port. No iPlug2, no SDKs; builds on Linux, macOS and Windows.
cmake_minimum_required(VERSION 3.14)
project(SergeMiddleDspTests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(SERGE_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

add_executable(test_wave_middle test_wave_middle.cpp)
target_compile_definitions(test_wave_middle PRIVATE SERGE_REPO_ROOT="${SERGE_REPO_ROOT}")
if(MSVC)
  target_compile_options(test_wave_middle PRIVATE /W4)
else()
  target_compile_options(test_wave_middle PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME test_wave_middle COMMAND test_wave_middle)
