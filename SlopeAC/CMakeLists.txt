cmake_minimum_required(VERSION 3.14)
project(SlopeAC VERSION 1.0.0)

# iPlug2 is the submodule at the repository root.
if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../iPlug2" CACHE PATH "iPlug2 root directory")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

# VST3, via Unix Makefiles.
iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    SlopeAC.cpp
    SlopeAC.h
    dsp/SecondHalf.h
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
  FORMATS VST3
)
