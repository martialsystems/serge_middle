cmake_minimum_required(VERSION 3.14)
project(SlopeAcDspTests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(SERGE_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

add_executable(test_second test_second.cpp)
target_compile_definitions(test_second PRIVATE SERGE_REPO_ROOT="${SERGE_REPO_ROOT}")
if(MSVC)
  target_compile_options(test_second PRIVATE /W4)
else()
  target_compile_options(test_second PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME test_second COMMAND test_second)
