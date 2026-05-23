#pragma once
#include <JuceHeader.h>

class MatrixPage : public juce::Component
{
public:
    MatrixPage (juce::AudioProcessorValueTreeState& apvts)
    {
        for (int src = 0; src < 2; ++src)
        {
            for (int dest = 0; dest < 2; ++dest)
            {
                // Create a rotary knob for this patch coordinate
                auto* slider = matrixSliders.add (new juce::Slider());
                slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 45, 15);
                addAndMakeVisible (slider);

                // Wire it straight to the dynamic MOD_X_Y parameter ID
                juce::String paramID = "MOD_" + juce::String (src) + "_" + juce::String (dest);
                matrixAttachments.add (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, *slider));
            }
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::darkgrey.darker (0.3f));
        g.setColour (juce::Colours::white);
        
        g.setFont (18.0f);
        g.drawText ("Modulation Matrix", getLocalBounds().removeFromTop (40), juce::Justification::centred);
        
        g.setColour (juce::Colours::lightgrey);
        g.setFont (12.0f);
        
        // Matrix Row Sources
        g.drawText ("From Op 1", 10, 85, 80, 20, juce::Justification::centredRight);
        g.drawText ("From Op 2", 10, 185, 80, 20, juce::Justification::centredRight);
        
        // Matrix Column Destinations
        g.drawText ("To Op 1", 120, 45, 70, 20, juce::Justification::centred);
        g.drawText ("To Op 2", 220, 45, 70, 20, juce::Justification::centred);
    }

    void resized() override
    {
        int index = 0;
        int startX = 120;
        int startY = 70;
        int spacing = 100;

        for (int src = 0; src < 2; ++src)
        {
            for (int dest = 0; dest < 2; ++dest)
            {
                if (auto* slider = matrixSliders[index++])
                {
                    slider->setBounds (startX + (dest * spacing), startY + (src * spacing), 70, 90);
                }
            }
        }
    }

private:
    juce::OwnedArray<juce::Slider> matrixSliders;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> matrixAttachments;
};
