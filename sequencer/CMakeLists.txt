cmake_minimum_required(VERSION 3.22)

# Define your project name and programming language
project(SUBHARMONIC_SEQUENCER VERSION 1.0.0 LANGUAGES CXX)

# JUCE 7 and 8 require at least C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Locate and include the JUCE framework subfolder
# This expects the JUCE repository to be inside your project directory
# add_subdirectory(JUCE)
# --- Automatically download and include JUCE ---
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.0
)
FetchContent_MakeAvailable(juce)

# 2. Declare your plugin target and vital characteristics
juce_add_plugin(SubharmonicSequencer
    COMPANY_NAME "CustomPlugins"
    PLUGIN_MANUFACTURER_CODE "CstP" # 4 characters, at least one uppercase
    PLUGIN_CODE "SbSq"             # 4 characters, exactly one uppercase
    FORMATS VST3                   # You can add AU or Standalone here if desired
    IS_MIDI_EFFECT TRUE            # Critical: Prevents the DAW from expecting audio processing
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    PRODUCT_NAME "Subharmonic Sequencer"
)

# 3. Supply the source files to compile
target_sources(SubharmonicSequencer PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
)

# --- ADD THIS BLOCK TO BYPASS GTK/WEBKIT DEPENDENCIES ---
target_compile_definitions(SubharmonicSequencer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# 4. Link the necessary JUCE module dependencies
target_link_libraries(SubharmonicSequencer PRIVATE
    juce::juce_audio_processors
)

# 5. Automatically handle boilerplate generation behind the scenes
juce_generate_juce_header(SubharmonicSequencer)
