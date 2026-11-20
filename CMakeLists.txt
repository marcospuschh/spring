cmake_minimum_required(VERSION 3.22)

if(APPLE)
    # binário universal (Apple Silicon + Intel), compatível com macOS 11+
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

project(PTZR_SPRING VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(PTZRSpring
    COMPANY_NAME "PTZR.PLUGINS"
    COMPANY_WEBSITE "https://ptzr.plugins"
    PLUGIN_MANUFACTURER_CODE Ptzr
    PLUGIN_CODE Spr1
    PRODUCT_NAME "PTZR Spring"
    BUNDLE_ID com.ptzrplugins.ptzrspring
    FORMATS VST3 AU Standalone
    VST3_CATEGORIES Fx Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(PTZRSpring PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/SpringEngine.h)

target_compile_definitions(PTZRSpring PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=0)

target_link_libraries(PTZRSpring
    PRIVATE juce::juce_audio_utils
    PUBLIC  juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
