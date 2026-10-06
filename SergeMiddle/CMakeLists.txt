cmake_minimum_required(VERSION 3.14)
project(SergeMiddle VERSION 1.0.0)

# iPlug2 is the submodule at the repository root.
if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../iPlug2" CACHE PATH "iPlug2 root directory")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

# VST3. The standalone APP compiles resources/SergeMiddle-macOS-MainMenu.xib
# with ibtool. This build uses the command line tools, so the format is VST3.
iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    SergeMiddle.cpp
    SergeMiddle.h
    dsp/WaveMiddle.h
    dsp/WaveMiddleTables.h
    resources/resource.h
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
    resources/pleat_plate.png
    resources/pleat_plate@2x.png
    resources/pleat_knob.png
    resources/pleat_knob@2x.png
  FORMATS VST3
)
