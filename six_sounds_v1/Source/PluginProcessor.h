#pragma once
#include <JuceHeader.h>
#include "SynthCore.h"

class MatrixFMSynthAudioProcessor : public juce::AudioProcessor {
public:
    MatrixFMSynthAudioProcessor();
    ~MatrixFMSynthAudioProcessor() override;
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SixSoundsFM"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void savePreset(); void loadPreset(); void initPreset(); void randomizeModMatrix();
    juce::StringArray getModDestinations();

    juce::AudioProcessorValueTreeState apvts;
    struct Visualizer : public juce::Component, public juce::Timer {
        juce::AudioBuffer<float> buffer;
        void pushBuffer(const juce::AudioBuffer<float>& b) { buffer = b; }
        void timerCallback() override { repaint(); }
        void paint(juce::Graphics& g) override {
            g.fillAll(juce::Colours::black); g.setColour(juce::Colours::cyan);
            if (buffer.getNumSamples() > 0) {
                juce::Path p; auto* data = buffer.getReadPointer(0);
                float w = (float)getWidth(), h = (float)getHeight();
                p.startNewSubPath(0, h/2);
                for(int i=0; i<buffer.getNumSamples(); i+=juce::jmax(1, (int)(buffer.getNumSamples()/w)))
                    p.lineTo(juce::jmap((float)i, 0.0f, (float)buffer.getNumSamples(), 0.0f, w), h/2 - (data[i] * h * 0.45f));
                g.strokePath(p, juce::PathStrokeType(1.5f));
            }
        }
    } visualizer;
private:
    juce::Synthesiser synth; std::unique_ptr<juce::FileChooser> fileChooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MatrixFMSynthAudioProcessor)
};
