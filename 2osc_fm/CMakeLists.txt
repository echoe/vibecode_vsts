cmake_minimum_required(VERSION 3.22)

project(2OscFM VERSION 1.0.0 LANGUAGES CXX)

# --- Automatically download and include JUCE ---
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.0
)
FetchContent_MakeAvailable(juce)
# ------------------------------------------------

juce_add_plugin(2OscFM
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
    PRODUCT_NAME "2OscFM")

target_sources(2OscFM PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)

# --- ADD THIS BLOCK TO BYPASS GTK/WEBKIT DEPENDENCIES ---
target_compile_definitions(2OscFM PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
# --------------------------------------------------------

target_compile_features(2OscFM PRIVATE cxx_std_17)

target_link_libraries(2OscFM PRIVATE
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_audio_utils
)

juce_generate_juce_header(2OscFM)
