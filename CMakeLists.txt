cmake_minimum_required(VERSION 3.25.0)

project(winesynth
    VERSION 1.0.0.1
    DESCRIPTION "WineSynth - Mono Oscillator VST3 Plugin"
)

set(vst3sdk_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vst3sdk")

# Cross-compilation: set output path and disable symlink creation
set(SMTG_PLUGIN_TARGET_USER_PATH "${CMAKE_BINARY_DIR}/VST3" CACHE PATH "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)

# Disable examples and validator
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
set(SMTG_CREATE_BUNDLE_FOR_WINDOWS ON CACHE BOOL "" FORCE)

# VSTGUI: disable standalone and UI scripting (MinGW incompatible)
set(VSTGUI_STANDALONE OFF CACHE BOOL "" FORCE)
set(VSTGUI_UISCRIPTING OFF CACHE BOOL "" FORCE)

add_subdirectory(${vst3sdk_SOURCE_DIR} ${CMAKE_BINARY_DIR}/vst3sdk)

set(plugin_sources
    source/processor.h
    source/processor.cpp
    source/controller.h
    source/controller.cpp
    source/editor.h
    source/editor.cpp
    source/controls.h
    source/pluginentry.cpp
    source/plugincids.h
    source/pluginparamids.h
    source/version.h
)

# Required for smtg_target_add_library_main to find dllmain.cpp
set(public_sdk_SOURCE_DIR "${vst3sdk_SOURCE_DIR}/public.sdk")

smtg_add_vst3plugin(winesynth ${plugin_sources})

target_compile_features(winesynth PUBLIC cxx_std_17)

target_link_libraries(winesynth
    PRIVATE
        sdk
        vstgui_support
        sdk       # repeated: vstgui_support needs symbols from sdk (MinGW link order)
        vstgui
        vstgui_uidescription
)

if(SMTG_WIN)
    target_link_libraries(winesynth
        PRIVATE
            d2d1 dwrite d3d11 dxgi dcomp dwmapi
            shlwapi imm32 opengl32
    )
endif()
