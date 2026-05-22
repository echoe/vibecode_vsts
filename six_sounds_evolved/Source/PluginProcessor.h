#pragma once
#include <JuceHeader.h>
#include "SynthEngine.h"

class NewFourOpSynthAudioProcessor  : public juce::AudioProcessor {
public:
    NewFourOpSynthAudioProcessor();
    ~NewFourOpSynthAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "4-Op Modular Synth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    void resetToInitPreset();

    juce::AudioProcessorValueTreeState apvts;
    AudioScopeBuffer scopeBuffer;

private:
    juce::Synthesiser synth;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateSynthParameters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewFourOpSynthAudioProcessor)
};
