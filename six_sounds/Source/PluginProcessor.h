// PluginProcessor.h
#pragma once
#include <JuceHeader.h>
#include "FMVoice.h"

// Simple dummy sound struct needed by juce::Synthesiser
struct FMSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class FMPluginAudioProcessor  : public juce::AudioProcessor
{
public:
    FMPluginAudioProcessor();
    ~FMPluginAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    //synth
    juce::Synthesiser synth;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateVoices();
    //effects
    juce::dsp::ProcessSpec spec;
    juce::dsp::Chorus<float> chorusModule;
    juce::dsp::Reverb        reverbModule;
    juce::dsp::Reverb::Parameters reverbParams;
    std::vector<std::vector<float>> delayBuffers;
    int delayWriteIndex = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FMPluginAudioProcessor)
};
