cmake_minimum_required(VERSION 3.22)

project(SpectralResonator VERSION 1.0.0 REQUIRED LANGUAGES CXX)

# Find JUCE (Assumes JUCE is installed globally or added as a submodule)
find_package(JUCE CONFIG REQUIRED)

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

target_compile_features(SpectralResonator PRIVATE cxx_std_17)

target_link_libraries(SpectralResonator PRIVATE
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_audio_utils
)

juce_generate_juce_header(SpectralResonator)
