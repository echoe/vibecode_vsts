// PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Constants.h"

FMPluginAudioProcessor::FMPluginAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // The curly braces are critical here so 'voice' exists in scope!
    // This is the number of voices the synth has (8 voice polyphony).
    for (int i = 0; i < 8; ++i)
    {
        auto* voice = new FMVoice();   // 1. Instantiate the voice
        voice->initParameters (apvts); // 2. Initialize parameters while it's in scope
        synth.addVoice (voice);        // 3. Hand ownership over to the JUCE synth
    }

    synth.addSound (new FMSound());
}

FMPluginAudioProcessor::~FMPluginAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout FMPluginAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    juce::StringArray waveChoices { "Sine", "Triangle", "Saw", "Square", "Additive", "Filter"};
    juce::StringArray filterTypeChoices { "Lowpass", "Highpass", "Bandpass", "Comb" };

    // Generate parameters for each operator dynamically
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        juce::String opNum = juce::String (i + 1);
        
        // Wave
	params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "WAVE_" + opNum, 1 }, "Op " + opNum + " Waveform", waveChoices, 0));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "FILTER_TYPE_" + opNum, 1 }, "Op " + opNum + " Filter Type", filterTypeChoices, 0));

	// Tuning Ratios, Phase, & Detune
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"RATIO_" + opNum, 1}, "Op " + opNum + " Ratio", 0.5f, 16.0f, 1.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"DETUNE_" + opNum, 1}, "Op " + opNum + " Detune", -50.0f, 50.0f, 0.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "PHASE_" + opNum, 1 }, "Op " + opNum + " Phase", 0.0f, 360.0f, 0.0f));

        // Envelopes
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"ATTACK_" + opNum, 1}, "Op " + opNum + " Attack", 0.001f, 5.0f, 0.1f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"DECAY_" + opNum, 1}, "Op " + opNum + " Decay", 0.01f, 5.0f, 0.2f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"SUSTAIN_" + opNum, 1}, "Op " + opNum + " Sustain", 0.0f, 1.0f, 0.8f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"RELEASE_" + opNum, 1}, "Op " + opNum + " Release", 0.01f, 5.0f, 0.5f));
        
        // Output Levels
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"OUT_" + opNum, 1}, "Op " + opNum + " Out Level", 0.0f, 1.0f, (i == 0) ? 1.0f : 0.0f));
    }

    // Generate Modulation Matrix Nodes (NxN Grid)
    for (int src = 0; src < ProjectConfig::numOperators; ++src)
    {
        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            juce::String paramID = "MOD_" + juce::String (src) + "_" + juce::String (dest);
            juce::String name = "Mod Op " + juce::String (src + 1) + " -> Op " + juce::String (dest + 1);
            params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {paramID, 1}, name, 0.0f, 10.0f, 0.0f));
        }
    }
    // 2. NEW: Audio Routing Matrix Nodes (NxN Grid)
    // This matrix determines how much raw audio signal passes from operator to operator
    for (int src = 0; src < ProjectConfig::numOperators; ++src)
    {
        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            juce::String paramID = "AUDIO_ROUTE_" + juce::String (src) + "_" + juce::String (dest);
            juce::String name = "Audio Matrix: Op " + juce::String (src + 1) + " -> Op " + juce::String (dest + 1);
            // Normalized gain between 0.0 (silent) and 1.0 (full volume pass-through)
            params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {paramID, 1}, name, 0.0f, 1.0f, 0.0f));
        }
    }

    // Add filter stuff
    juce::StringArray filterModes { "Bypass", "Lowpass", "Highpass", "Comb" };

    for (int f = 1; f <= ProjectConfig::numFilters; ++f)
    {
        juce::String fNum = juce::String (f);
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "FILTER" + fNum + "_MODE", 1 }, "Filter " + fNum + " Mode", filterModes, 0));
        params.push_back (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { "FILTER" + fNum + "_FREQ", 1 }, "Filter " + fNum + " Freq", 20.0f, 20000.0f, 2000.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { "FILTER" + fNum + "_RES",  1 }, "Filter " + fNum + " Res / FB", 0.0f, 0.95f, 0.1f));
    }
    // Limiter
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
    juce::ParameterID { "LIMITER_CEIL", 1 }, 
    "Limiter Ceiling", 
    -12.0f, 0.0f, -0.2f)); // Ranges from -12dB to 0dB, default safely at -0.2dB
    // This line has to be the end of this function, otherwise stuff won't process
    return { params.begin(), params.end() };
}

void FMPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    masterLimiter.prepare (sampleRate);
    
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
            voice->setCurrentPlaybackSampleRate (sampleRate); // Make sure your FMVoice class handles this parameter update!
    }
}

void FMPluginAudioProcessor::releaseResources() {}

void FMPluginAudioProcessor::updateVoices()
{
    // Read current atomic parameters from APVTS and safely pass them to our synthesis engine
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
        {
            // In a production build, add a thread-safe method inside `FMVoice` 
            // (e.g., voice->updateParameters(...)) to read these parameters atomically.
        }
    }
}

void FMPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    updateVoices();
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
    // Get limiter setting and pass it into the processing cycle
    float ceilDb = apvts.getRawParameterValue ("LIMITER_CEIL")->load (std::memory_order_relaxed);
    float ceilLinear = juce::Decibels::decibelsToGain (ceilDb);
    masterLimiter.processBlock (buffer, ceilLinear);
}

juce::AudioProcessorEditor* FMPluginAudioProcessor::createEditor()
{
    return new FMPluginAudioProcessorEditor (*this);
}

void FMPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void FMPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

// This function fixes your 'undefined reference to createPluginFilter()' error!
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FMPluginAudioProcessor();
}
