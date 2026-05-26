#pragma once
#include <JuceHeader.h>

class EffectsPage  : public juce::Component
{
public:
    EffectsPage (juce::AudioProcessorValueTreeState& vts) : apvts (vts)
    {
        // Helper lambda to rapidly construct rotary FX sliders and map them to APVTS
        auto setupParam = [this] (juce::Slider& slider, juce::Label& label, 
                                  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                                  const juce::String& paramID, const juce::String& labelText)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 55, 16);
            addAndMakeVisible (slider);

            label.setText (labelText, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (label);

            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);
        };

        // Initialize Chorus UI
        setupParam (chorusMixSlider, chorusMixLabel, chorusMixAttach, "CHORUS_MIX", "Mix");
        setupParam (chorusRateSlider, chorusRateLabel, chorusRateAttach, "CHORUS_RATE", "Rate");
        setupParam (chorusDepthSlider, chorusDepthLabel, chorusDepthAttach, "CHORUS_DEPTH", "Depth");

        // Initialize Delay UI
        setupParam (delayMixSlider, delayMixLabel, delayMixAttach, "DELAY_MIX", "Mix");
        setupParam (delayTimeSlider, delayTimeLabel, delayTimeAttach, "DELAY_TIME", "Time");
        setupParam (delayFeedbackSlider, delayFeedbackLabel, delayFeedbackAttach, "DELAY_FEEDBACK", "Feedback");

        // Initialize Reverb UI
        setupParam (reverbMixSlider, reverbMixLabel, reverbMixAttach, "REVERB_MIX", "Mix");
        setupParam (reverbRoomSlider, reverbRoomLabel, reverbRoomAttach, "REVERB_ROOM", "Room");
    }

    ~EffectsPage() override {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::transparentBlack);
        auto bounds = getLocalBounds().toFloat();

        // Separate the page visually into 3 clean vertical modules
        float sectionWidth = bounds.getWidth() / 3.0f;
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        
        g.fillRoundedRectangle (bounds.getX() + 10, bounds.getY() + 10, sectionWidth - 20, bounds.getHeight() - 20, 6.0f);
        g.fillRoundedRectangle (bounds.getX() + sectionWidth + 10, bounds.getY() + 10, sectionWidth - 20, bounds.getHeight() - 20, 6.0f);
        g.fillRoundedRectangle (bounds.getX() + (sectionWidth * 2) + 10, bounds.getY() + 10, sectionWidth - 20, bounds.getHeight() - 20, 6.0f);

        // Header Labels
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
	g.drawText ("CHORUS", juce::Rectangle<int>(0, 20, sectionWidth, 30), juce::Justification::centred);
        g.drawText ("DELAY", juce::Rectangle<int>(sectionWidth, 20, sectionWidth, 30), juce::Justification::centred);
        g.drawText ("REVERB", juce::Rectangle<int>(sectionWidth * 2, 20, sectionWidth, 30), juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (15);
        int sectionWidth = area.getWidth() / 3;
        
        // --- Lay out Chorus Knobs (Column 1) ---
        auto chorusArea = area.removeFromLeft (sectionWidth).withTrimmedTop (50);
        int knobHeight = chorusArea.getHeight() / 3;
        
        auto layoutRow = [knobHeight] (juce::Slider& slider, juce::Label& label, juce::Rectangle<int>& column) {
            auto row = column.removeFromTop (knobHeight);
            label.setBounds (row.removeFromTop (18));
            slider.setBounds (row.reduced (5));
        };

        layoutRow (chorusMixSlider, chorusMixLabel, chorusArea);
        layoutRow (chorusRateSlider, chorusRateLabel, chorusArea);
        layoutRow (chorusDepthSlider, chorusDepthLabel, chorusArea);

        // --- Lay out Delay Knobs (Column 2) ---
        auto delayArea = area.removeFromLeft (sectionWidth).withTrimmedTop (50);
        layoutRow (delayMixSlider, delayMixLabel, delayArea);
        layoutRow (delayTimeSlider, delayTimeLabel, delayArea);
        layoutRow (delayFeedbackSlider, delayFeedbackLabel, delayArea);

        // --- Lay out Reverb Knobs (Column 3) ---
        auto reverbArea = area.withTrimmedTop (50);
        layoutRow (reverbMixSlider, reverbMixLabel, reverbArea);
        layoutRow (reverbRoomSlider, reverbRoomLabel, reverbArea);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;

    // UI Modules UI components
    juce::Slider chorusMixSlider, chorusRateSlider, chorusDepthSlider;
    juce::Label  chorusMixLabel, chorusRateLabel, chorusDepthLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusMixAttach, chorusRateAttach, chorusDepthAttach;

    juce::Slider delayMixSlider, delayTimeSlider, delayFeedbackSlider;
    juce::Label  delayMixLabel, delayTimeLabel, delayFeedbackLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayMixAttach, delayTimeAttach, delayFeedbackAttach;

    juce::Slider reverbMixSlider, reverbRoomSlider;
    juce::Label  reverbMixLabel, reverbRoomLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbMixAttach, reverbRoomAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsPage)
};
