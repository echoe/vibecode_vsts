#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Top bar Oscilloscope Component
class ScopeComponent : public juce::Component, private juce::Timer {
public:
    ScopeComponent(AudioScopeBuffer& b) : buffer(b) { startTimerHz(30); }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::limegreen);
        std::array<float, 256> scopeData;
        buffer.readSamples(scopeData);
        
        juce::Path p;
        float h = static_cast<float>(getHeight());
        float w = static_cast<float>(getWidth());
        p.startNewSubPath(0, h / 2.0f + scopeData[0] * (h / 2.0f));
        
        for (size_t i = 1; i < scopeData.size(); ++i) {
            p.lineTo((static_cast<float>(i) / scopeData.size()) * w, h / 2.0f + scopeData[i] * (h / 2.0f));
        }
        g.strokePath(p, juce::PathStrokeType(2.0f));
    }
private:
    void timerCallback() override { repaint(); }
    AudioScopeBuffer& buffer;
};

// UI Row element mapping individual operators dynamically
class OpRowComponent : public juce::Component, private juce::ComboBox::Listener {
public:
    OpRowComponent(int index, juce::AudioProcessorValueTreeState& state);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void comboBoxChanged(juce::ComboBox* box) override;

private:
    int opIdx;
    juce::AudioProcessorValueTreeState& apvts;
    juce::String prefix;

    juce::ComboBox waveModeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;

    // Dynamic Options Holders
    juce::ComboBox soundTypeCombo, resKeyCombo, filtTypeCombo;
    juce::Slider ratioSlider, pwSlider, qSlider, dampSlider, resSlider, cutoffSlider;
    std::array<juce::Slider, 8> partialSliders;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttaches;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttaches;

    // Global Envelope / Output Elements
    juce::Slider attackSl, decaySl, sustainSl, releaseSl, outputSl;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> globalAttaches;

    void setupDynamicComponents();
    void updateVisibleParameters();
};

class OscillatorsPage : public juce::Component {
public:
    OscillatorsPage(juce::AudioProcessorValueTreeState& state) {
        for (int i = 0; i < numOps; ++i) {
            rows.add(std::make_unique<OpRowComponent>(i, state));
            addAndMakeVisible(rows.getLast());
        }
    }
    void resized() override {
        auto area = getLocalBounds();
        auto rowH = area.getHeight() / numOps;
        for (auto* row : rows) row->setBounds(area.removeFromTop(rowH).reduced(0, 2));
    }
private:
    juce::OwnedArray<OpRowComponent> rows;
};

class ModulationPage : public juce::Component {
public:
    ModulationPage(juce::AudioProcessorValueTreeState& state);
    void resized() override;
private:
    std::array<std::array<juce::Slider, numOps>, numOps> matrixSliders;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> matrixAttaches;
    
    // Twin independent LFO UI groups
    struct LfoUi {
        juce::ComboBox target, type;
        juce::Slider rate;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tarAt, typAt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratAt;
    } lfoUis[2];
};

class NewFourOpSynthAudioProcessorEditor  : public juce::AudioProcessorEditor, private juce::Button::Listener {
public:
    NewFourOpSynthAudioProcessorEditor (NewFourOpSynthAudioProcessor&);
    ~NewFourOpSynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void buttonClicked(juce::Button* b) override;

private:
    NewFourOpSynthAudioProcessor& audioProcessor;
    
    juce::Label presetLabel;
    ScopeComponent scopeComponent;
    juce::Slider masterVolSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterVolAttach;

    juce::TextButton oscPageBtn{"Oscillators"}, modPageBtn{"Modulation"};
    OscillatorsPage oscPage;
    ModulationPage modPage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewFourOpSynthAudioProcessorEditor)
};
