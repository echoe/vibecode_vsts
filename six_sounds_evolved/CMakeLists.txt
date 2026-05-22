cmake_minimum_required(VERSION 3.22)

project(FourOpSynth VERSION 1.0.0 LANGUAGES CXX)

# --- Automatically download and include JUCE ---
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.0
)
FetchContent_MakeAvailable(juce)

# Initialize the JUCE plugin target architecture
juce_add_plugin(FourOpSynth
    COMPANY_NAME "YourCompany"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Fops
    PLUGIN_CODE Fosy
    FORMATS VST3 Standalone
)

# This generates the JuceHeader.h file that our source files are looking for.
juce_generate_juce_header(FourOpSynth)

# Allocate target source files
target_sources(FourOpSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/SynthEngine.h
)

# Force standard up-to-date C++ modules
target_compile_features(FourOpSynth PRIVATE cxx_std_20)

# --- ADD THIS BLOCK TO BYPASS GTK/WEBKIT DEPENDENCIES ---
target_compile_definitions(FourOpSynth PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
# --------------------------------------------------------

# Link essential JUCE engine modules required by our plugin
target_link_libraries(FourOpSynth PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_data_structures
    juce::juce_events
    juce::juce_core
)
