// OperatorsPage.h
#pragma once
#include <JuceHeader.h>
#include "Constants.h" // Make sure ProjectConfig is visible

struct CompactOperatorGroup : public juce::Component
{
    CompactOperatorGroup (juce::AudioProcessorValueTreeState& apvts, int opIndex)
    {
        juce::String opNum = juce::String (opIndex + 1);
    
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
        setupSlider (levelSlider, levelLabel, "Level", true);
    
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
    
        waveSelector.clear (juce::dontSendNotification);
        waveSelector.addItemList ({ "Sine", "Triangle", "Saw", "Square", "Additive" , "Filter" }, 1);
        addAndMakeVisible (waveSelector);
        
        filterTypeSelector.clear (juce::dontSendNotification);
        filterTypeSelector.addItemList ({ "Lowpass", "Highpass", "Bandpass", "Comb" }, 1);
        addAndMakeVisible (filterTypeSelector);
    
        // Secure APVTS Links
	syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "TEMPO_SYNC_" + opNum, syncButton);
        ratioAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "RATIO_" + opNum, ratioSlider);
        detuneAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "DETUNE_" + opNum, detuneSlider);
        phaseAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "PHASE_" + opNum, phaseSlider);
        levelAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "OUT_" + opNum, levelSlider);
    
        attackAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "ATTACK_" + opNum, attackSlider);
        decayAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "DECAY_" + opNum, decaySlider);
        sustainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "SUSTAIN_" + opNum, sustainSlider);
        releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "RELEASE_" + opNum, releaseSlider);
        waveAttach    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "WAVE_" + opNum, waveSelector);
        filterTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "FILTER_TYPE_" + opNum, filterTypeSelector);
   
        // Dynamic Visibility Rule Logic using safe value captures
        waveSelector.onChange = [this, &apvts, opNum]()
        {
            updateKnobFunction(apvts, opNum);
        };

	// Dynamic logic when the Sync Button is toggled
        syncButton.onClick = [this]()
        {
            bool isSynced = syncButton.getToggleState();
            if (isSynced)
            {
                // Remap Ratio slider to act as time subdivisions / percentages
                ratioLabel.setText ("Sync Rate", juce::dontSendNotification);
                ratioSlider.setTextValueSuffix ("x");
            }
            else
            {
                // Restore standard FM Ratio label
                bool isFilterMode = (waveSelector.getSelectedId() == 6);
                ratioLabel.setText (isFilterMode ? "Cutoff" : "Ratio", juce::dontSendNotification);
                ratioSlider.setTextValueSuffix ("");
            }
        };
    
        // Trigger initial state assignment safely
        updateKnobFunction(apvts, opNum);
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
        opHeaderLabel.setBounds (topStrip.removeFromLeft (85));
	syncButton.setBounds (topStrip.removeFromLeft (45).reduced (1));
        
        if (filterTypeSelector.isVisible())
        {
            int menuWidth = topStrip.getWidth() / 2;
            waveSelector.setBounds (topStrip.removeFromLeft (menuWidth).reduced (1));
            filterTypeSelector.setBounds (topStrip.reduced (1));
        }
        else
        {
            waveSelector.setBounds (topStrip.reduced (1));
        }

        auto knobZone = area.removeFromTop (area.getHeight() / 2 + 5);
        int knobWidth = knobZone.getWidth() / 4;

        auto rArea = knobZone.removeFromLeft (knobWidth); ratioLabel.setBounds (rArea.removeFromTop (15)); ratioSlider.setBounds (rArea);
        auto dArea = knobZone.removeFromLeft (knobWidth); detuneLabel.setBounds (dArea.removeFromTop (15)); detuneSlider.setBounds (dArea);
        auto pArea = knobZone.removeFromLeft (knobWidth); phaseLabel.setBounds (pArea.removeFromTop (15));  phaseSlider.setBounds (pArea);
        auto lArea = knobZone; levelLabel.setBounds (lArea.removeFromTop (15)); levelSlider.setBounds (lArea);

        int sliderWidth = area.getWidth() / 4;
        auto aArea = area.removeFromLeft (sliderWidth);   attackLabel.setBounds (aArea.removeFromTop (20));   attackSlider.setBounds (aArea);
        auto decArea = area.removeFromLeft (sliderWidth);  decayLabel.setBounds (decArea.removeFromTop (20)); decaySlider.setBounds (decArea);
        auto sArea = area.removeFromLeft (sliderWidth);    sustainLabel.setBounds (sArea.removeFromTop (20)); sustainSlider.setBounds (sArea);
        auto relArea = area;                              releaseLabel.setBounds (relArea.removeFromTop (20)); releaseSlider.setBounds (relArea);
    }

private:
    // Dedicated method handles the attachment mutation cleanly
    void updateKnobFunction (juce::AudioProcessorValueTreeState& apvts, const juce::String& opNum)
    {
        bool isFilterMode = (waveSelector.getSelectedId() == 6);
        filterTypeSelector.setVisible (isFilterMode);
    
        // Fixed: Use phaseAttach instead of phaseAttachment
        phaseAttach.reset();
    
        if (isFilterMode)
        {
            ratioLabel.setText ("Cutoff", juce::dontSendNotification);
            detuneLabel.setText ("Resolution", juce::dontSendNotification);
            phaseLabel.setText ("Q", juce::dontSendNotification);
    
            phaseSlider.setRange (0.1, 10.0, 0.01);
            phaseSlider.setSkewFactorFromMidPoint (1.5); 
            phaseSlider.setTextValueSuffix ("");
    
            phaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, "FILTER_Q_" + opNum, phaseSlider);
        }
        else
        {
            ratioLabel.setText ("Ratio", juce::dontSendNotification);
            detuneLabel.setText ("Detune", juce::dontSendNotification);
            phaseLabel.setText ("Phase", juce::dontSendNotification);
    
            phaseSlider.setRange (0.0, 360.0, 1.0);
            phaseSlider.setSkewFactor (1.0); 
            phaseSlider.setTextValueSuffix (juce::CharPointer_UTF8 ("°"));
    
            phaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, "PHASE_" + opNum, phaseSlider);
        }
    
        if (getWidth() > 0 && getHeight() > 0)
        {
            resized();
        }
    }

    juce::Slider ratioSlider, detuneSlider, levelSlider, phaseSlider;
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::ComboBox waveSelector;
    juce::ComboBox filterTypeSelector;
    juce::Label opHeaderLabel;
    juce::Label ratioLabel, detuneLabel, levelLabel, phaseLabel;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;

    juce::ToggleButton syncButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttach, detuneAttach, levelAttach, phaseAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach, decayAttach, sustainAttach, releaseAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttachment;
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
