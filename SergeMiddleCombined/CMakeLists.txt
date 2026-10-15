cmake_minimum_required(VERSION 3.14)
project(SergeMiddleCombined VERSION 1.0.0)

# iPlug2 is the submodule at the repository root.
if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../iPlug2" CACHE PATH "iPlug2 root directory")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

# VST3, via Unix Makefiles. The plate is drawn for this window.
# Knobs, the switch, and Roboto are the PLEAT and Slope AC art.
iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    SergeMiddleCombined.cpp
    SergeMiddleCombined.h
    PatchView.h
    dsp/Patch.h
  RESOURCES
    resources/middle_plate.png
    resources/middle_plate@2x.png
    ../SergeMiddle/resources/fonts/Roboto-Regular.ttf
    ../SergeMiddle/resources/pleat_knob.png
    ../SergeMiddle/resources/pleat_knob@2x.png
    ../SlopeAC/resources/slope_switch.png
    ../SlopeAC/resources/slope_switch@2x.png
    ../SlopeAC/resources/slope_route.png
    ../SlopeAC/resources/slope_route@2x.png
  FORMATS VST3
)
