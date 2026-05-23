#pragma once
#include <JuceHeader.h>

// A reusable UI group for a single operator's parameters
struct OperatorControlGroup : public juce::Component
{
    OperatorControlGroup (juce::AudioProcessorValueTreeState& apvts, int opIndex)
    {
        juce::String opNum = juce::String (opIndex + 1);

        // Quick lambda to style and label our rotary knobs consistently
        auto setupSlider = [this] (juce::Slider& slider, juce::Label& label, const juce::String& text)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 15);
            addAndMakeVisible (slider);

            label.setText (text, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (label);
        };

        // 1. Initialize and label tuning & master level controls
        setupSlider (ratioSlider, ratioLabel, "Ratio");
        setupSlider (detuneSlider, detuneLabel, "Detune");
        setupSlider (levelSlider, levelLabel, "Level");

        // 2. Initialize and label individual envelope stages
        setupSlider (attackSlider, attackLabel, "Attack");
        setupSlider (decaySlider, decayLabel, "Decay");
        setupSlider (sustainSlider, sustainLabel, "Sustain");
        setupSlider (releaseSlider, releaseLabel, "Release");

	// Set up the Waveform Dropdown Menu
        waveSelector.addItemList ({ "Sine", "Triangle", "Saw", "Square" }, 1);
        addAndMakeVisible (waveSelector);

        waveLabel.setText ("Waveform", juce::dontSendNotification);
        waveLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (waveLabel);

        // 3. Bind everything to the APVTS tree using safe attachments
        ratioAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "RATIO_" + opNum, ratioSlider);
        detuneAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "DETUNE_" + opNum, detuneSlider);
        levelAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "OUT_" + opNum, levelSlider);
        attackAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "ATTACK_" + opNum, attackSlider);
        decayAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "DECAY_" + opNum, decaySlider);
        sustainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "SUSTAIN_" + opNum, sustainSlider);
        releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "RELEASE_" + opNum, releaseSlider);
	waveAttach    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "WAVE_" + opNum, waveSelector);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        // Allocate space for the dropdown menu at the very top of the group control panel
        auto waveArea = area.removeFromTop (30);
        waveLabel.setBounds (waveArea.removeFromLeft (70));
        waveSelector.setBounds (waveArea.reduced (2));
        int rowHeight = area.getHeight() / 2;
        
        // Top half of the panel: Oscillator characteristics
        auto oscRow = area.removeFromTop (rowHeight);
        int oscWidth = oscRow.getWidth() / 3;
        
        auto rArea = oscRow.removeFromLeft (oscWidth).reduced (5);
        ratioLabel.setBounds (rArea.removeFromTop (15));
        ratioSlider.setBounds (rArea);
        
        auto dArea = oscRow.removeFromLeft (oscWidth).reduced (5);
        detuneLabel.setBounds (dArea.removeFromTop (15));
        detuneSlider.setBounds (dArea);
        
        auto lArea = oscRow.reduced (5);
        levelLabel.setBounds (lArea.removeFromTop (15));
        levelSlider.setBounds (lArea);

        // Bottom half of the panel: Envelope settings
        int envWidth = area.getWidth() / 4;
        
        auto aArea = area.removeFromLeft (envWidth).reduced (2);
        attackLabel.setBounds (aArea.removeFromTop (15));
        attackSlider.setBounds (aArea);
        
        auto decArea = area.removeFromLeft (envWidth).reduced (2);
        decayLabel.setBounds (decArea.removeFromTop (15));
        decaySlider.setBounds (decArea);
        
        auto sArea = area.removeFromLeft (envWidth).reduced (2);
        sustainLabel.setBounds (sArea.removeFromTop (15));
        sustainSlider.setBounds (sArea);
        
        auto relArea = area.reduced (2);
        releaseLabel.setBounds (relArea.removeFromTop (15));
        releaseSlider.setBounds (relArea);
    }

private:
    juce::Slider ratioSlider, detuneSlider, levelSlider;
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::ComboBox waveSelector;
    juce::Label ratioLabel, detuneLabel, levelLabel;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;
    juce::Label waveLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttach, detuneAttach, levelAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach, decayAttach, sustainAttach, releaseAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
};

class OperatorsPage : public juce::Component
{
public:
    OperatorsPage (juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < 2; ++i)
        {
            auto* opGroup = opGroups.add (new OperatorControlGroup (apvts, i));
            addAndMakeVisible (opGroup);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::darkgrey.darker (0.2f));
        g.setColour (juce::Colours::orange.brighter (0.4f));
        g.setFont (15.0f);
        
        auto area = getLocalBounds();
        g.drawText ("OPERATOR 1 (Carrier)", area.removeFromLeft (getWidth() / 2).removeFromTop (30), juce::Justification::centred);
        g.drawText ("OPERATOR 2 (Modulator)", area.removeFromTop (30), juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromTop (35); // Title bar offset
        
        auto leftArea = area.removeFromLeft (getWidth() / 2).reduced (10);
        auto rightArea = area.reduced (10);
        
        if (opGroups[0]) opGroups[0]->setBounds (leftArea);
        if (opGroups[1]) opGroups[1]->setBounds (rightArea);
    }

private:
    juce::OwnedArray<OperatorControlGroup> opGroups;
};
