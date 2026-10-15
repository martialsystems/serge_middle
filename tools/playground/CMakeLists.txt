# Headless patch playground. No iPlug2, no plugin SDK.
# Copyright (c) 2026 Martial Systems LLC. All rights reserved.
cmake_minimum_required(VERSION 3.14)
project(SergePlayground CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(playground src/playground.cpp)
target_include_directories(playground PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../..")

if(MSVC)
  target_compile_options(playground PRIVATE /W4)
else()
  target_compile_options(playground PRIVATE -Wall -Wextra -Wpedantic)
endif()
