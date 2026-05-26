// MatrixPage.h
#pragma once
#include <JuceHeader.h>
#include "Constants.h"

// --- VERTICAL ASSIGNMENT LANE ---
struct FocusedModMatrixRow : public juce::Component
{
    FocusedModMatrixRow (juce::AudioProcessorValueTreeState& vts, int rowIndex)
        : apvts (vts), rowNum (rowIndex)
    {
        oscSelector.addItemList ({ "Src: 1", "Src: 2", "Src: 3", "Src: 4", "Src: 5", "Src: 6" }, 1);
        oscSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (oscSelector);

        targetSelector.addItemList ({ "Pitch", "Phase", "Level", "Cutoff", "Res" }, 1);
        targetSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (targetSelector);

        targetSelector.onChange = [this]() { updateSliderAttachments(); };
        oscSelector.onChange    = [this]() { updateSliderAttachments(); };

        targetAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "ROW_TARGET_" + juce::String (rowNum + 1), targetSelector);

        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            sliders[i].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            sliders[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 12);
            addAndMakeVisible (sliders[i]);

            labels[i].setText ("-> Op " + juce::String (i + 1), juce::dontSendNotification);
            labels[i].setJustificationType (juce::Justification::centred);
            labels[i].setFont (juce::FontOptions (10.0f));
            addAndMakeVisible (labels[i]);
        }

        updateSliderAttachments();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        
        // Stack selectors vertically at the top of the column lane
        auto controlsArea = area.removeFromTop (48);
        oscSelector.setBounds (controlsArea.removeFromTop (22).reduced (2, 1));
        targetSelector.setBounds (controlsArea.reduced (2, 1));

        // Carve remaining vertical space into 6 slots going top-to-bottom
        int knobHeight = area.getHeight() / ProjectConfig::numOperators;
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            auto cell = area.removeFromTop (knobHeight);
            labels[i].setBounds (cell.removeFromTop (12));
            sliders[i].setBounds (cell.reduced (1));
        }
    }

    void updateSliderAttachments()
    {
        int srcIndex = oscSelector.getSelectedId() - 1;
        if (srcIndex < 0) srcIndex = 0;

        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            sliderAttach[dest].reset();

            juce::String paramID = "FOCUSED_MOD_R" + juce::String (rowNum + 1) + "_" + 
                                   juce::String (srcIndex) + "_" + juce::String (dest);

            sliderAttach[dest] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, paramID, sliders[dest]);
        }
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    int rowNum;

    juce::ComboBox oscSelector;
    juce::ComboBox targetSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttach;

    std::array<juce::Slider, ProjectConfig::numOperators> sliders;
    std::array<juce::Label, ProjectConfig::numOperators> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, ProjectConfig::numOperators> sliderAttach;
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

        if (paramPrefix == "MOD_")
        {
            for (int row = 0; row < 3; ++row)
            {
                matrixRows.push_back (std::make_unique<FocusedModMatrixRow> (apvts, row));
                addAndMakeVisible (*matrixRows.back());
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

        if (paramPrefix == "MOD_" && !matrixRows.empty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.15f));
            float splitX = static_cast<float> (matrixRows[0]->getX() - 12);
            g.drawVerticalLine (static_cast<int>(splitX), 45.0f, static_cast<float>(getHeight() - 10));
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();

        juce::Rectangle<int> mainGridArea;
        juce::Rectangle<int> rightSidebar;

        if (paramPrefix == "MOD_")
        {
            // Dedicate 65% space to NxN matrix, 35% to the vertical lanes block
            mainGridArea = area.removeFromLeft (static_cast<int> (area.getWidth() * 0.65));
            rightSidebar = area;
        }
        else
        {
            mainGridArea = area;
        }

        // only drop header for main grid area
	area.removeFromTop (40);

        int index = 0;
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
        {
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
            {
                if (auto* slider = matrixSliders[index++])
                {
                    slider->setBounds (mainGridArea.getX() + ProjectConfig::matrixXPos + (dest * ProjectConfig::matrixSpacing), 
                                       ProjectConfig::matrixYPos + (src * ProjectConfig::matrixSpacing), 
                                       70, 90);
                }
            }
        }

        if (paramPrefix == "MOD_")
        {
            rightSidebar.removeFromLeft (12); // Split divider line offset padding
            
            // Layout the 3 strips side-by-side inside the sidebar panel width
            int laneWidth = rightSidebar.getWidth() / 3;
            for (size_t i = 0; i < matrixRows.size(); ++i)
            {
                matrixRows[i]->setBounds (rightSidebar.removeFromLeft (laneWidth).reduced (4, 0));
            }
        }
    }

private:
    juce::String paramPrefix;
    juce::String matrixTitle;

    juce::OwnedArray<juce::Slider> matrixSliders;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> matrixAttachments;

    std::vector<std::unique_ptr<FocusedModMatrixRow>> matrixRows;
};
