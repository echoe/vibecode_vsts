#pragma once

#include <JuceHeader.h>

class SubharmonicSequencerAudioProcessor  : public juce::AudioProcessor
{
public:
    SubharmonicSequencerAudioProcessor();
    ~SubharmonicSequencerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Helper to create all the parameters needed for 8 steps dynamically
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;

    // Internal state tracking
    double lastPpqPosition = -1.0;
    
    // Struct to track a note that is currently sounding so we can shut it off accurately
    struct PendingNoteOff {
        int midiNote;
        double offPpqPosition;
    };
    std::vector<PendingNoteOff> activeNoteOffs;

    // Helper functions
    int calculateSubharmonicNote (int mainNote, int divisionFactor);
    void handleMidiTriggers (double startPpq, double endPpq, double bpm, juce::MidiBuffer& midiMessages);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubharmonicSequencerAudioProcessor)
};
