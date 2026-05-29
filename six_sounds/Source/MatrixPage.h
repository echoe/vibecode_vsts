// MatrixPage.h
#pragma once
#include <JuceHeader.h>
#include "Constants.h"

// --- 6-SLOT MODULATION MATRIX ROW ---
struct ModMatrixSlot : public juce::Component
{
    ModMatrixSlot (juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    {
        juce::String slotNum = juce::String (slotIndex + 1);

        // 1. Source Selector
        sourceSelector.addItemList ({ "None", "LFO 1", "LFO 2", "Env 1", "Env 2", "Velocity", "Mod Wheel" }, 1);
        sourceSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (sourceSelector);

        // 2. Target Selector
        targetSelector.addItemList ({ "None", "Op 1 Pitch", "Op 2 Pitch", "Op 1 Level", "Cutoff", "Resonance", "Master Pitch" }, 1);
        targetSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (targetSelector);

        // 3. Amount Slider (Bi-polar -1.0 to 1.0)
        amountSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        amountSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 15);
        addAndMakeVisible (amountSlider);

        // 4. Parameter Attachments
        srcAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "MOD_SRC_" + slotNum, sourceSelector);
            
        tgtAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "MOD_TGT_" + slotNum, targetSelector);
            
        amtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "MOD_AMT_" + slotNum, amountSlider);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (4);
        
        // Split the row into 3 horizontal pieces: Source -> Target -> Amount
        int third = area.getWidth() / 3;
        
        sourceSelector.setBounds (area.removeFromLeft (third).reduced (2));
        targetSelector.setBounds (area.removeFromLeft (third).reduced (2));
        amountSlider.setBounds (area.reduced (2));
    }

private:
    juce::ComboBox sourceSelector;
    juce::ComboBox targetSelector;
    juce::Slider amountSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tgtAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAttach;
};

// --- MAIN REUSABLE MATRIX PAGE COMPONENT ---
class MatrixPage : public juce::Component
{
public:
    MatrixPage (juce::AudioProcessorValueTreeState& apvts, 
                const juce::String& prefix, 
                const juce::String& title)
        : paramPrefix (prefix), 
          matrixTitle (title)
    {
        // 1. Setup Main NxN Grid Sliders
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
        {
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
            {
                auto* slider = matrixSliders.add (new juce::Slider());
                slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 45, 15);
                addAndMakeVisible (slider);

                juce::String paramID = paramPrefix + juce::String (src) + "_" + juce::String (dest);
                matrixAttachments.add (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, *slider));
            }
        }

        // 2. Setup the Right Sidebar Controls based on the current Page
        if (paramPrefix == "MOD_")
        {
            // If Mod Page: Generate 6 slots for the routing matrix
            for (int i = 0; i < 6; ++i)
            {
                modSlots.push_back (std::make_unique<ModMatrixSlot> (apvts, i));
                addAndMakeVisible (*modSlots.back());
            }
        }
        else 
        {
            // If Audio/FM Routing Page: Generate Master Output Levels for the Carriers
            for (int i = 0; i < ProjectConfig::numOperators; ++i)
            {
                auto* outSlider = outputSliders.add (new juce::Slider());
                outSlider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                outSlider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 12);
                addAndMakeVisible (outSlider);

                auto* outLabel = outputLabels.add (new juce::Label());
                outLabel->setText ("Out " + juce::String (i + 1), juce::dontSendNotification);
                outLabel->setJustificationType (juce::Justification::centred);
                addAndMakeVisible (outLabel);

                // These use the "OUT_1", "OUT_2" APVTS parameters you used to have on the operator page
                outputAttachments.add (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    apvts, "OUT_" + juce::String (i + 1), *outSlider));
            }
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::darkgrey.darker (0.3f));
        g.setColour (juce::Colours::white);

        g.setFont (18.0f);
        g.drawText (matrixTitle, getLocalBounds().removeFromTop (40), juce::Justification::centred);

        g.setColour (juce::Colours::lightgrey);
        g.setFont (12.0f);

        // Draw Matrix Grid Labels
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
        {
            int yPos = ProjectConfig::matrixYPos - 10 + (src * ProjectConfig::matrixSpacing) + (ProjectConfig::matrixSpacing / 2);
            g.drawText ("From Op " + juce::String (src + 1),
                        ProjectConfig::matrixXPos - ProjectConfig::matrixSpacing, yPos, 80, 20,
                        juce::Justification::centredRight);
        }

        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            int xPos = ProjectConfig::matrixXPos + (dest * ProjectConfig::matrixSpacing);
            g.drawText ("To Op " + juce::String (dest + 1),
                        xPos - 5, ProjectConfig::matrixYPos - 10, 80, 20,
                        juce::Justification::centred);
        }

        // Sidebar Splitter Line
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        float splitX = static_cast<float> (getLocalBounds().getWidth() * 0.65f);
        g.drawVerticalLine (static_cast<int>(splitX), 45.0f, static_cast<float>(getHeight() - 10));
        
        // Sidebar Header Text
        g.setColour (juce::Colours::white.withAlpha(0.6f));
        g.setFont (14.0f);
        g.drawText (paramPrefix == "MOD_" ? "MODULATION MATRIX" : "CARRIER OUTPUT LEVELS", 
                    static_cast<int>(splitX) + 10, 30, getWidth() - static_cast<int>(splitX) - 20, 20, 
                    juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromTop (50); // Make room for titles

        // Split into Main Grid (65%) and Sidebar (35%)
        auto mainGridArea = area.removeFromLeft (static_cast<int> (area.getWidth() * 0.65));
        auto rightSidebar = area.reduced(10, 0); // Add some padding

        // 1. Layout Main NxN Grid
        int index = 0;
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
        {
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
            {
                if (auto* slider = matrixSliders[index++])
                {
                    slider->setBounds (mainGridArea.getX() + ProjectConfig::matrixXPos + (dest * ProjectConfig::matrixSpacing), 
                                       ProjectConfig::matrixYPos + (src * ProjectConfig::matrixSpacing), // adjusted for top cut
                                       70, 90);
                }
            }
        }

        // 2. Layout Sidebar Contextually
        if (paramPrefix == "MOD_")
        {
            // Stack the 6 mod slots vertically
            int slotHeight = rightSidebar.getHeight() / 6;
            for (auto& slot : modSlots)
            {
                slot->setBounds (rightSidebar.removeFromTop (slotHeight).reduced(0, 2));
            }
        }
	else
        {
            int rowHeight = rightSidebar.getHeight() / ProjectConfig::numOperators;
        
            for (int i = 0; i < ProjectConfig::numOperators; ++i)
            {
                // Carve out a row from the top
                auto cell = rightSidebar.removeFromTop (rowHeight).reduced (0, 2);
        
                // Squeeze the label to the left side of the row
                if (auto* label = outputLabels[i])
                    label->setBounds (cell.removeFromLeft (50));
        
                // The fader stretches horizontally across the rest of the row space
                if (auto* slider = outputSliders[i])
                    slider->setBounds (cell);
            }
        }
    }

private:
    juce::String paramPrefix;
    juce::String matrixTitle;

    // NxN Matrix Components
    juce::OwnedArray<juce::Slider> matrixSliders;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> matrixAttachments;

    // Mod Page Context Components
    std::vector<std::unique_ptr<ModMatrixSlot>> modSlots;
    
    // Audio Routing Context Components
    juce::OwnedArray<juce::Slider> outputSliders;
    juce::OwnedArray<juce::Label> outputLabels;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachments;
};
