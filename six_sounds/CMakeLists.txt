cmake_minimum_required(VERSION 3.22)

project(SixSounds VERSION 1.0.0 LANGUAGES C CXX)

# --- Automatically download and include JUCE ---
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.13
)
FetchContent_MakeAvailable(juce)
# ------------------------------------------------

juce_add_plugin(SixSounds
    COMPANY_NAME "VibecodeXtreme"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE "YCom"
    PLUGIN_PLUGIN_CODE "Sres"
    FORMATS VST3 AU
    PRODUCT_NAME "SixSounds")

target_sources(SixSounds PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)

# --- ADD THIS BLOCK TO BYPASS GTK/WEBKIT DEPENDENCIES ---
target_compile_definitions(SixSounds PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
# --------------------------------------------------------

target_compile_features(SixSounds PRIVATE cxx_std_17)

target_link_libraries(SixSounds PRIVATE
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_audio_utils
)

juce_generate_juce_header(SixSounds)
