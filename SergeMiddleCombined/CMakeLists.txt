cmake_minimum_required(VERSION 3.14)
project(SergeMiddleCombined VERSION 1.0.0)

# iPlug2 is the submodule at the repository root.
if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../iPlug2" CACHE PATH "iPlug2 root directory")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

# VST3, via Unix Makefiles. The plate is the hand-drawn SLEWFOLD face.
# Knobs are the filmstrip. Roboto is the readout face.
iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    SergeMiddleCombined.cpp
    SergeMiddleCombined.h
    PatchView.h
    CableRope.h
    CableEnds.h
    dsp/Patch.h
    AppMenuBootstrap.mm
  RESOURCES
    resources/slewfold_plate.png
    resources/slewfold_plate@2x.png
    resources/knob_filmstrip.png
    resources/knob_filmstrip@2x.png
    ../SergeMiddle/resources/fonts/Roboto-Regular.ttf
  FORMATS VST3 APP
)

# Standalone app: build the altered IPlugAPP_host.cpp in app/ (input channel
# count follows the device) instead of the stock one.
if(TARGET ${PROJECT_NAME}-app)
  set_source_files_properties(${IPLUG2_DIR}/IPlug/APP/IPlugAPP_host.cpp PROPERTIES HEADER_FILE_ONLY TRUE)
  target_sources(${PROJECT_NAME}-app PRIVATE app/IPlugAPP_host.cpp)
  set_source_files_properties(app/IPlugAPP_host.cpp PROPERTIES LANGUAGE OBJCXX)
endif()
