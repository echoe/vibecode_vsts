cmake_minimum_required(VERSION 3.15)
project(SixSounds VERSION 0.0.2)

# Add JUCE (Make sure the JUCE folder is inside SixSounds)
add_subdirectory(JUCE)

# Linux Dependencies. These need to be removed to build this on windows or mac
find_package(PkgConfig REQUIRED)
pkg_check_modules(FT2 REQUIRED freetype2)
pkg_check_modules(CURL REQUIRED libcurl)

# Required JUCE Boilerplate.
juce_add_plugin(SixSounds
    COMPANY_NAME "echoe_vibecode"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    NEEDS_WEB_BROWSER TRUE
    JUCE_LOAD_CURL_SYMBOLS_LAZILY TRUE
    FORMATS VST3 Standalone)

#Set compile definitions.
target_compile_definitions(SixSounds PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0   # Fixes the VST3 conflict error
    JUCE_WEB_BROWSER=0             # Optional: slightly faster build
    JUCE_USE_CURL=1                # Ensures CURL is active for the build
)

# Source Files
target_sources(SixSounds PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/SynthCore.h)

# Link Libraries & Include Directories
target_include_directories(SixSounds PRIVATE 
    ${FT2_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS})

target_link_libraries(SixSounds PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_graphics
    ${FT2_LIBRARIES}
    ${CURL_LIBRARIES})

juce_generate_juce_header(SixSounds)
