
cmake_minimum_required(VERSION 3.22)

project(MixBalancePro VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE
set(JUCE_DIR "${CMAKE_SOURCE_DIR}/JUCE" CACHE PATH "Path to JUCE")

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_DIR}")
endif()

add_subdirectory("${JUCE_DIR}" JUCE)

# Plugin
juce_add_plugin(MixBalancePro
    FORMATS VST3
    PRODUCT_NAME "MixBalancePro"
    COMPANY_NAME "AshleAudio"
    VERSION ${PROJECT_VERSION}
    PLUGIN_CODE MbPr
    PLUGIN_MANUFACTURER_CODE Ashl
    IS_SYNTH FALSE
    IS_MIDI_EFFECT FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
)

# Generate JuceHeader.h
juce_generate_juce_header(MixBalancePro)

# Sources
target_sources(MixBalancePro PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/Analysis/LoudnessAnalyzer.cpp
    Source/Analysis/ConflictDetector.cpp
    Source/Analysis/DynamicRangeAnalyzer.cpp
    Source/Analysis/RecommendationEngine.cpp
    Source/Analysis/SpectrumAnalyzer.cpp

    Source/UI/MainComponent.cpp
    Source/UI/Visualization/LoudnessMeter.cpp
    Source/UI/Visualization/ConflictHeatmap.cpp
    Source/UI/Visualization/SpectrumDisplay.cpp
    Source/UI/Visualization/DynamicRangeDisplay.cpp
    Source/UI/Panels/RecommendationPanel.cpp
    Source/UI/Panels/TrackListPanel.cpp

    Source/DSP/FFTProcessor.cpp
    Source/DSP/AudioBufferUtils.cpp

    Source/Utils/ParameterManager.cpp
    Source/Utils/PresetManager.cpp
)

# Include directories
target_include_directories(MixBalancePro PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Analysis
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/UI
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Visualization
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Panels
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Utils
)

# Resources
juce_add_binary_data(MixBalanceProResources
    SOURCES
        Resources/Icon.png
        Resources/LookAndFeel.json
)

target_link_libraries(MixBalancePro PRIVATE
    MixBalanceProResources

    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# Definitions
target_compile_definitions(MixBalancePro PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DONT_AUTO_LINK_TO_UUID_LIBRARY=1
)

# Install
install(TARGETS MixBalancePro
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
