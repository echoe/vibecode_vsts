cmake_minimum_required(VERSION 3.22)

project(SpectralResonator VERSION 1.0.0 LANGUAGES CXX)

# --- Automatically download and include JUCE ---
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.0
)
FetchContent_MakeAvailable(juce)
# ------------------------------------------------

juce_add_plugin(SpectralResonator
    COMPANY_NAME "YourCompany"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE "YCom"
    PLUGIN_PLUGIN_CODE "Sres"
    FORMATS VST3 AU
    PRODUCT_NAME "Spectral Resonator")

target_sources(SpectralResonator PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
)

# --- ADD THIS BLOCK TO BYPASS GTK/WEBKIT DEPENDENCIES ---
target_compile_definitions(SpectralResonator PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
# --------------------------------------------------------

target_compile_features(SpectralResonator PRIVATE cxx_std_17)

target_link_libraries(SpectralResonator PRIVATE
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_audio_utils
)

juce_generate_juce_header(SpectralResonator)
