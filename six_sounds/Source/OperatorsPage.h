// OperatorsPage.h
#pragma once
#include <JuceHeader.h>
#include "Constants.h"

struct CompactOperatorGroup : public juce::Component
{
    CompactOperatorGroup (juce::AudioProcessorValueTreeState& valueTreeState, int opIndex)
        : apvts (valueTreeState), opNum (juce::String (opIndex + 1))
    {
        auto setupSlider = [this] (juce::Slider& slider, juce::Label& label, const juce::String& text, bool rotary)
        {
            slider.setSliderStyle (rotary ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearVertical);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 12);
            addAndMakeVisible (slider);
            
            label.setText (text, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (label);
        };
    
        setupSlider (ratioSlider, ratioLabel, "Ratio", true);
        setupSlider (detuneSlider, detuneLabel, "Detune", true);
        setupSlider (phaseSlider, phaseLabel, "Phase", true);
        setupSlider (foldSlider, foldLabel, "Fold", true);
    
        setupSlider (attackSlider, attackLabel, "Attack", false);
        setupSlider (decaySlider, decayLabel, "Decay", false);
        setupSlider (sustainSlider, sustainLabel, "Suspend", false);
        setupSlider (releaseSlider, releaseLabel, "Release", false);
    
        opHeaderLabel.setText ("OPERATOR " + opNum, juce::dontSendNotification);
        opHeaderLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (opHeaderLabel);

        syncButton.setButtonText ("Sync");
        syncButton.setClickingTogglesState (true);
        addAndMakeVisible (syncButton);
        
        modeSelector.addItemList ({ "Wave", "Additive", "Filter" }, 1);
        addAndMakeVisible (modeSelector);

        waveShapeSelector.addItemList ({ "Sine", "Triangle", "Saw", "Square", "White Noise" }, 1);
        addAndMakeVisible (waveShapeSelector);
        
        filterTypeSelector.addItemList ({ "Lowpass", "Highpass", "Bandpass", "Comb" }, 1);
        addAndMakeVisible (filterTypeSelector);

        // APVTS Links
        syncAttach    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "TEMPO_SYNC_" + opNum, syncButton);
        ratioAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "RATIO_" + opNum, ratioSlider);
        detuneAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "DETUNE_" + opNum, detuneSlider);
        phaseAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "PHASE_" + opNum, phaseSlider);
        foldAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "FOLD_" + opNum, foldSlider);
    
        attackAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "ATTACK_" + opNum, attackSlider);
        decayAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "DECAY_" + opNum, decaySlider);
        sustainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "SUSTAIN_" + opNum, sustainSlider);
        releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "RELEASE_" + opNum, releaseSlider);
        
        modeAttach      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "MODE_" + opNum, modeSelector);
        waveShapeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "WAVE_SHAPE_" + opNum, waveShapeSelector);
        filterTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "FILTER_TYPE_" + opNum, filterTypeSelector);

        // Safe UI state triggers using the stored class member reference
        modeSelector.onChange = [this]() { updateUIState(); };
        syncButton.onClick    = [this]() { updateUIState(); };
    
        updateUIState();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.02f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (6);
        auto topStrip = area.removeFromTop (22);
        // 1. Position the Header and Sync Button
        opHeaderLabel.setBounds (topStrip.removeFromLeft (85));
        syncButton.setBounds (topStrip.removeFromLeft (45).reduced (1));
        // 2. FIX: Carve out 75 pixels for the Mode Selector dropdown!
        modeSelector.setBounds (topStrip.removeFromLeft (75).reduced (1));
        // 3. The remaining space goes to the conditional dropdowns
        if (filterTypeSelector.isVisible())
        {
            int menuWidth = topStrip.getWidth() / 2;
            waveShapeSelector.setBounds (topStrip.removeFromLeft (menuWidth).reduced (1));
            filterTypeSelector.setBounds (topStrip.reduced (1));
        }
        else
        {
            waveShapeSelector.setBounds (topStrip.reduced (1));
        }
    
        auto knobZone = area.removeFromTop (area.getHeight() / 2 + 5);
        int knobWidth = knobZone.getWidth() / 4;
    
        auto rArea = knobZone.removeFromLeft (knobWidth); ratioLabel.setBounds (rArea.removeFromTop (15)); ratioSlider.setBounds (rArea);
        auto dArea = knobZone.removeFromLeft (knobWidth); detuneLabel.setBounds (dArea.removeFromTop (15)); detuneSlider.setBounds (dArea);
        auto pArea = knobZone.removeFromLeft (knobWidth); phaseLabel.setBounds (pArea.removeFromTop (15));  phaseSlider.setBounds (pArea);
        auto lArea = knobZone; foldLabel.setBounds (lArea.removeFromTop (15)); foldSlider.setBounds (lArea);
    
        int sliderWidth = area.getWidth() / 4;
        auto aArea = area.removeFromLeft (sliderWidth);   attackLabel.setBounds (aArea.removeFromTop (20));   attackSlider.setBounds (aArea);
        auto decArea = area.removeFromLeft (sliderWidth);  decayLabel.setBounds (decArea.removeFromTop (20)); decaySlider.setBounds (decArea);
        auto sArea = area.removeFromLeft (sliderWidth);    sustainLabel.setBounds (sArea.removeFromTop (20)); sustainSlider.setBounds (sArea);
        auto relArea = area;                              releaseLabel.setBounds (relArea.removeFromTop (20)); releaseSlider.setBounds (relArea);
    }

private:
    // Combined logic method avoids layout text fighting
    void updateUIState()
    {
        int selectedMode = modeSelector.getSelectedId();
        bool isWaveMode = (selectedMode == 1);
        bool isFilterMode = (selectedMode == 3);
        bool isSynced = syncButton.getToggleState();

        waveShapeSelector.setVisible (isWaveMode);
        filterTypeSelector.setVisible (isFilterMode);

        // Handle dynamic parameter re-linking safely via class reference
        phaseAttach.reset();

        if (isFilterMode)
        {
            ratioLabel.setText (isSynced ? "Sync Rate" : "Cutoff", juce::dontSendNotification);
            detuneLabel.setText ("Resonance", juce::dontSendNotification);
            phaseLabel.setText ("Q", juce::dontSendNotification);
	    foldLabel.setText("Feedback", juce::dontSendNotification);

            phaseSlider.setRange (0.1, 10.0, 0.01);
            phaseSlider.setSkewFactorFromMidPoint (1.5);
            phaseSlider.setTextValueSuffix ("");

            phaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "FILTER_Q_" + opNum, phaseSlider);
        }
        else
        {
            ratioLabel.setText (isSynced ? "Sync Rate" : "Ratio", juce::dontSendNotification);
            detuneLabel.setText ("Detune", juce::dontSendNotification);
            phaseLabel.setText ("Phase", juce::dontSendNotification);
	    foldLabel.setText ("Fold", juce::dontSendNotification);

            phaseSlider.setRange (0.0, 360.0, 1.0);
            phaseSlider.setSkewFactor (1.0);
            phaseSlider.setTextValueSuffix (juce::CharPointer_UTF8 ("°"));

            phaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "PHASE_" + opNum, phaseSlider);
        }

        ratioSlider.setTextValueSuffix (isSynced ? "x" : "");

        if (getWidth() > 0 && getHeight() > 0)
            resized();
    }

    // Keep state reference completely safe inside class lifecycle
    juce::AudioProcessorValueTreeState& apvts;
    juce::String opNum;

    juce::Slider ratioSlider, detuneSlider, phaseSlider, foldSlider;
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    
    juce::Label opHeaderLabel;
    juce::Label ratioLabel, detuneLabel, phaseLabel, foldLabel;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;
    
    juce::ComboBox modeSelector;
    juce::ComboBox waveShapeSelector;
    juce::ComboBox filterTypeSelector;
    juce::ToggleButton syncButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveShapeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttach, detuneAttach, phaseAttach, foldAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach, decayAttach, sustainAttach, releaseAttach;
};

// --- THE PARENT VIEW MANAGER CLASS ---
class OperatorsPage : public juce::Component
{
public:
    OperatorsPage (juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            opModules.push_back (std::make_unique<CompactOperatorGroup> (apvts, i));
            addAndMakeVisible (*opModules.back());
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();

        int rows = 2;
        int cols = 3;
        int cellWidth = area.getWidth() / cols;
        int cellHeight = area.getHeight() / rows;

        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                int index = (r * cols) + c;
                if (index < static_cast<int>(opModules.size()))
                {
                    opModules[index]->setBounds (c * cellWidth, r * cellHeight, cellWidth, cellHeight);
                }
            }
        }
    }

private:
    std::vector<std::unique_ptr<CompactOperatorGroup>> opModules;
};
