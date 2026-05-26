// OperatorsPage.h
#pragma once
#include <JuceHeader.h>

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
    
        setupSlider (ratioSlider, ratioLabel, "Rat", true);
        setupSlider (detuneSlider, detuneLabel, "Det", true);
        setupSlider (levelSlider, levelLabel, "Lvl", true);
        setupSlider (phaseSlider, phaseLabel, "Phs", true);
    
        setupSlider (attackSlider, attackLabel, "A", false);
        setupSlider (decaySlider, decayLabel, "D", false);
        setupSlider (sustainSlider, sustainLabel, "S", false);
        setupSlider (releaseSlider, releaseLabel, "R", false);
    
        opHeaderLabel.setText ("OPERATOR " + opNum, juce::dontSendNotification);
        opHeaderLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (opHeaderLabel);
    
        waveSelector.clear (juce::dontSendNotification);
        waveSelector.addItemList ({ "Sine", "Triangle", "Saw", "Square", "Additive" , "Filter" }, 1);
        addAndMakeVisible (waveSelector);
        
        filterTypeSelector.clear (juce::dontSendNotification);
        filterTypeSelector.addItemList ({ "Lowpass", "Highpass", "Bandpass", "Comb" }, 1);
        addAndMakeVisible (filterTypeSelector);
    
        // Secure APVTS Links
        ratioAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "RATIO_" + opNum, ratioSlider);
        detuneAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "DETUNE_" + opNum, detuneSlider);
        levelAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "OUT_" + opNum, levelSlider);
        phaseAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "PHASE_" + opNum, phaseSlider);
    
        attackAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "ATTACK_" + opNum, attackSlider);
        decayAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "DECAY_" + opNum, decaySlider);
        sustainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "SUSTAIN_" + opNum, sustainSlider);
        releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "RELEASE_" + opNum, releaseSlider);
        waveAttach    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "WAVE_" + opNum, waveSelector);
        filterTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "FILTER_TYPE_" + opNum, filterTypeSelector);
    
        // Dynamic Visibility Rule Logic
        waveSelector.onChange = [this]()
        {
            bool isFilterMode = (waveSelector.getSelectedId() == 6);
            filterTypeSelector.setVisible (isFilterMode);
            
            if (isFilterMode)
            {
                ratioLabel.setText ("Cut", juce::dontSendNotification);
                detuneLabel.setText ("Res", juce::dontSendNotification);
            }
            else
            {
                ratioLabel.setText ("Rat", juce::dontSendNotification);
                detuneLabel.setText ("Det", juce::dontSendNotification);
            }
            
            // Safety Guard: Only trigger explicit redraws if the component is actually alive on screen
            if (getWidth() > 0 && getHeight() > 0)
            {
                resized(); 
            }
        };
    
        // Trigger initial state assignment safely without running mathematical bounds loops
        bool isFilterMode = (waveSelector.getSelectedId() == 6);
        filterTypeSelector.setVisible (isFilterMode);
    }

    void paint (juce::Graphics& g) override
    {
        // Draw a clean, subtle outline boundary around each operator module block
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.02f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f);
    }

    void resized() override
    {
        // Use aggressive pixel reductions to reclaim every bit of space
        auto area = getLocalBounds().reduced (6);

        // 1. Top Header Strip: Name on left, menus split balance space on right
        auto topStrip = area.removeFromTop (22);
        opHeaderLabel.setBounds (topStrip.removeFromLeft (85));
        
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

        // 2. Middle Strip: Place the 4 knobs right against each other
        auto knobZone = area.removeFromTop (area.getHeight() / 2 + 5);
        int knobWidth = knobZone.getWidth() / 4;

        auto rArea = knobZone.removeFromLeft (knobWidth); ratioLabel.setBounds (rArea.removeFromTop (11)); ratioSlider.setBounds (rArea);
        auto dArea = knobZone.removeFromLeft (knobWidth); detuneLabel.setBounds (dArea.removeFromTop (11)); detuneSlider.setBounds (dArea);
        auto pArea = knobZone.removeFromLeft (knobWidth); phaseLabel.setBounds (pArea.removeFromTop (11));  phaseSlider.setBounds (pArea);
        auto lArea = knobZone; levelLabel.setBounds (lArea.removeFromTop (11)); levelSlider.setBounds (lArea);

        // 3. Bottom Strip: Place the 4 sliders side-by-side
        int sliderWidth = area.getWidth() / 4;
        auto aArea = area.removeFromLeft (sliderWidth);   attackLabel.setBounds (aArea.removeFromTop (11));   attackSlider.setBounds (aArea);
        auto decArea = area.removeFromLeft (sliderWidth);  decayLabel.setBounds (decArea.removeFromTop (11)); decaySlider.setBounds (decArea);
        auto sArea = area.removeFromLeft (sliderWidth);    sustainLabel.setBounds (sArea.removeFromTop (11)); sustainSlider.setBounds (sArea);
        auto relArea = area;                              releaseLabel.setBounds (relArea.removeFromTop (11)); releaseSlider.setBounds (relArea);
    }

private:
    juce::Slider ratioSlider, detuneSlider, levelSlider, phaseSlider;
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::ComboBox waveSelector;
    juce::ComboBox filterTypeSelector;
    juce::Label opHeaderLabel;
    juce::Label ratioLabel, detuneLabel, levelLabel, phaseLabel;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttach, detuneAttach, levelAttach, phaseAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach, decayAttach, sustainAttach, releaseAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttachment;
};

// --- THE PARENT VIEW MANAGER CLASS ---
class OperatorsPage : public juce::Component
{
public:
    // Replaced the loop instantiation code with standard JUCE formatting loops
    OperatorsPage (juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            // Instantiate and display all 6 compact sub-modules
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

        // Dynamically slice the window coordinates into a fast 3x2 hardware matrix
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
