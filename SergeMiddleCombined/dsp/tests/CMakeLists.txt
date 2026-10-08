cmake_minimum_required(VERSION 3.14)
project(SergeMiddleCombinedDspTests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(SERGE_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

add_executable(test_patch test_patch.cpp)
target_compile_definitions(test_patch PRIVATE SERGE_REPO_ROOT="${SERGE_REPO_ROOT}")
if(MSVC)
  target_compile_options(test_patch PRIVATE /W4)
else()
  target_compile_options(test_patch PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME test_patch COMMAND test_patch)
