#pragma once
#include <JuceHeader.h>

class FiltersPage : public juce::Component
{
public:
    FiltersPage (juce::AudioProcessorValueTreeState& apvts)
    {
        auto setupFilterUI = [this, &apvts] (int id, juce::ComboBox& menu, juce::Slider& fSlider, juce::Slider& rSlider, juce::Label& fLabel, juce::Label& rLabel, const juce::String& name)
        {
            juce::String prefix = "FILTER" + juce::String (id);
            
            menu.addItemList ({ "Bypass", "Lowpass", "Highpass", "Comb" }, 1);
            addAndMakeVisible (menu);
            
            fSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            fSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
            addAndMakeVisible (fSlider);
            fLabel.setText ("Frequency", juce::dontSendNotification);
            fLabel.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (fLabel);

            rSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            rSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
            addAndMakeVisible (rSlider);
            rLabel.setText ("Res / Feedback", juce::dontSendNotification);
            rLabel.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (rLabel);

            menuAttach.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, prefix + "_MODE", menu));
            sliderAttach.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, prefix + "_FREQ", fSlider));
            sliderAttach.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, prefix + "_RES", rSlider));
        };

        setupFilterUI (1, f1Menu, f1Freq, f1Res, f1FreqLabel, f1ResLabel, "Filter 1");
        setupFilterUI (2, f2Menu, f2Freq, f2Res, f2FreqLabel, f2ResLabel, "Filter 2");
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (16.0f);
        g.drawText ("FILTER 1", 0, 10, getWidth() / 2, 20, juce::Justification::centred);
        g.drawText ("FILTER 2", getWidth() / 2, 10, getWidth() / 2, 20, juce::Justification::centred);
        
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawVerticalLine (getWidth() / 2, 10.0f, static_cast<float> (getHeight() - 10));
    }

    void resized() override
    {
        auto area = getLocalBounds().removeFromBottom (getHeight() - 40);
        int halfWidth = area.getWidth() / 2;

        auto layoutSection = [] (int leftBound, int width, juce::ComboBox& menu, juce::Slider& f, juce::Slider& r, juce::Label& fl, juce::Label& rl, int height) {
            menu.setBounds (leftBound + 20, 10, width - 40, 24);
            int knobWidth = width / 2;
            
            fl.setBounds (leftBound, 45, knobWidth, 15);
            f.setBounds (leftBound, 60, knobWidth, height - 70);
            
            rl.setBounds (leftBound + knobWidth, 45, knobWidth, 15);
            r.setBounds (leftBound + knobWidth, 60, knobWidth, height - 70);
        };

        layoutSection (0, halfWidth, f1Menu, f1Freq, f1Res, f1FreqLabel, f1ResLabel, area.getHeight());
        layoutSection (halfWidth, halfWidth, f2Menu, f2Freq, f2Res, f2FreqLabel, f2ResLabel, area.getHeight());
    }

private:
    juce::ComboBox f1Menu, f2Menu;
    juce::Slider f1Freq, f1Res, f2Freq, f2Res;
    juce::Label f1FreqLabel, f1ResLabel, f2FreqLabel, f2ResLabel;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> menuAttach;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttach;
};
