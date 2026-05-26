// MatrixPage.h
#pragma once
#include <JuceHeader.h>
#include "Constants.h"

class MatrixPage : public juce::Component
{
public:
    // The constructor now expects the VTS context, the parameter ID prefix, and the visual page title
    MatrixPage (juce::AudioProcessorValueTreeState& apvts, 
                const juce::String& prefix, 
                const juce::String& title)
        : paramPrefix (prefix), 
          matrixTitle (title)
    {
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
        {
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
            {
                // Create a rotary knob for this patch coordinate
                auto* slider = matrixSliders.add (new juce::Slider());
                slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 45, 15);
                addAndMakeVisible (slider);

                // Wire it straight to the dynamic parameter ID ("MOD_X_Y" or "AUDIO_ROUTE_X_Y")
                juce::String paramID = paramPrefix + juce::String (src) + "_" + juce::String (dest);
                matrixAttachments.add (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, *slider));
            }
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::darkgrey.darker (0.3f));
        g.setColour (juce::Colours::white);

        g.setFont (18.0f);
        // Uses the dynamic title variable passed via the constructor
        g.drawText (matrixTitle, getLocalBounds().removeFromTop (40), juce::Justification::centred);

        g.setColour (juce::Colours::lightgrey);
        g.setFont (12.0f);

        // Matrix Row Sources. Defaults declared in Constants.h
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
        {
            // Calculation: Starts at matrixXPos, increases by ProjectConfig::matrixSpacing for each operator
            int yPos = ProjectConfig::matrixYPos - 10 + ( src * ProjectConfig::matrixSpacing) + ( ProjectConfig::matrixSpacing / 2 );

            g.drawText ("From Op " + juce::String (src + 1),
                        ProjectConfig::matrixXPos - ProjectConfig::matrixSpacing, yPos, 80, 20,
                        juce::Justification::centredRight);
        }

        // --- Matrix Column Destinations ---
        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            // Calculation: Starts at ProjectConfig::matrixYPos, increases by ProjectCofig::matrixSpacing for each operator
            int xPos = ProjectConfig::matrixXPos + (dest * ProjectConfig::matrixSpacing);

            g.drawText ("To Op " + juce::String (dest + 1),
                        xPos - 5, ProjectConfig::matrixYPos - 10, 80, 20,
                        juce::Justification::centred);
        }
    }

    void resized() override
    {
        int index = 0;
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
        {
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
            {
                if (auto* slider = matrixSliders[index++])
                {
                    slider->setBounds (ProjectConfig::matrixXPos + (dest * ProjectConfig::matrixSpacing), ProjectConfig::matrixYPos + (src * ProjectConfig::matrixSpacing), 70, 90);
                }
            }
        }
    }

private:
    // Cached configuration strings
    juce::String paramPrefix;
    juce::String matrixTitle;

    juce::OwnedArray<juce::Slider> matrixSliders;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> matrixAttachments;
};
