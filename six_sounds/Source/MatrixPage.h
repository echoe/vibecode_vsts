#pragma once
#include <JuceHeader.h>
#include "Constants.h"

// Shared choice lists — keep these in sync with PluginProcessor.cpp
namespace ModChoices
{
    inline juce::StringArray sources()
    {
        return { "None", "Op 1", "Op 2", "Op 3", "Op 4", "Op 5", "Op 6" };
    }

    inline juce::StringArray targets()
    {
        return {
            "None",
            "Op 1 Ratio",  "Op 1 Detune", "Op 1 Phase", "Op 1 Fold", "Op 1 Level",
            "Op 2 Ratio",  "Op 2 Detune", "Op 2 Phase", "Op 2 Fold", "Op 2 Level",
            "Op 3 Ratio",  "Op 3 Detune", "Op 3 Phase", "Op 3 Fold", "Op 3 Level",
            "Op 4 Ratio",  "Op 4 Detune", "Op 4 Phase", "Op 4 Fold", "Op 4 Level",
            "Op 5 Ratio",  "Op 5 Detune", "Op 5 Phase", "Op 5 Fold", "Op 5 Level",
            "Op 6 Ratio",  "Op 6 Detune", "Op 6 Phase", "Op 6 Fold", "Op 6 Level",
            "Chorus Mix",   "Chorus Rate",  "Chorus Depth",
            "Delay Mix",    "Delay Time",   "Delay Feedback",
            "Reverb Mix",   "Reverb Room"
        };
    }
}

// --- ONE ROW OF THE MODULATION MATRIX ---
struct ModMatrixSlot : public juce::Component
{
    ModMatrixSlot (juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    {
        juce::String s = juce::String (slotIndex + 1); // slots are 1-indexed in param IDs

        // Source dropdown — items are 1-indexed in ComboBox, 0-indexed in AudioParameterChoice
        // JUCE's ComboBoxAttachment handles the offset automatically
        sourceSelector.addItemList (ModChoices::sources(), 1);
        sourceSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (sourceSelector);

        targetSelector.addItemList (ModChoices::targets(), 1);
        targetSelector.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (targetSelector);

        // Bi-polar amount slider
        amountSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        amountSlider.setRange (-1.0, 1.0, 0.001);
        amountSlider.setValue (0.0, juce::dontSendNotification);
        amountSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 15);
        addAndMakeVisible (amountSlider);

        // Row label
        rowLabel.setText ("S" + s, juce::dontSendNotification);
        rowLabel.setJustificationType (juce::Justification::centred);
        rowLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
	addAndMakeVisible (rowLabel);

        // Attach to APVTS — param IDs must match PluginProcessor exactly
        srcAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "MOD_SRC_" + s, sourceSelector);
        tgtAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, "MOD_TGT_" + s, targetSelector);
        amtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "MOD_AMT_" + s, amountSlider);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (2, 3);

        // Layout: [S#] [Source Dropdown] [Target Dropdown] [Amount Slider]
        rowLabel.setBounds       (area.removeFromLeft (20));
        sourceSelector.setBounds (area.removeFromLeft (90).reduced (2, 0));
        targetSelector.setBounds (area.removeFromLeft (110).reduced (2, 0));
        amountSlider.setBounds   (area.reduced (2, 0));
    }

    void paint (juce::Graphics& g) override
    {
        // Subtle alternating row tint — slotIndex is determined by position in parent
        g.fillAll (juce::Colours::black.withAlpha (0.08f));
    }

private:
    juce::Label    rowLabel;
    juce::ComboBox sourceSelector, targetSelector;
    juce::Slider   amountSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAttach, tgtAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   amtAttach;
};


// --- MAIN MATRIX PAGE ---
class MatrixPage : public juce::Component
{
public:
    MatrixPage (juce::AudioProcessorValueTreeState& apvts,
                const juce::String& prefix,
                const juce::String& title)
        : paramPrefix (prefix), matrixTitle (title)
    {
        // NxN FM grid — param IDs must be "FM_src_dest" (or whatever prefix+"src_dest")
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
        {
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
            {
                auto* s = matrixSliders.add (new juce::Slider());
                s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 45, 13);
                addAndMakeVisible (s);

                // e.g. "FM_0_1" or "MOD_0_1" depending on which page this is
                juce::String id = paramPrefix + juce::String (src) + "_" + juce::String (dest);
                matrixAttachments.add (
                    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *s));
            }
        }

        // Sidebar: mod slots (MOD_ page) or output levels (FM_ page)
        if (paramPrefix == "MOD_")
        {
            for (int i = 0; i < 6; ++i)
            {
                modSlots.push_back (std::make_unique<ModMatrixSlot> (apvts, i));
                addAndMakeVisible (*modSlots.back());
            }
        }
        else
        {
            for (int i = 0; i < ProjectConfig::numOperators; ++i)
            {
                auto* sl = outputSliders.add (new juce::Slider());
                sl->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                sl->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 12);
                addAndMakeVisible (sl);

                auto* lb = outputLabels.add (new juce::Label());
                lb->setText ("Out " + juce::String (i + 1), juce::dontSendNotification);
                lb->setJustificationType (juce::Justification::centred);
                addAndMakeVisible (lb);

                outputAttachments.add (
                    std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                        apvts, "OUT_" + juce::String (i + 1), *sl));
            }
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::darkgrey.darker (0.3f));

        // Title
        g.setColour (juce::Colours::white);
        g.setFont (18.0f);
        g.drawText (matrixTitle, getLocalBounds().removeFromTop (40), juce::Justification::centred);

        // Row/column labels for the NxN grid
        g.setColour (juce::Colours::lightgrey);
        g.setFont (11.0f);

        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            // Row labels (left side — "From Op N")
            int y = gridY + i * cellSize + cellSize / 2 - 8;
            g.drawText ("Op " + juce::String (i + 1),
                        gridX - 38, y, 36, 16,
                        juce::Justification::centredRight);

            // Column labels (top — "To Op N")
            int x = gridX + i * cellSize;
            g.drawText ("Op " + juce::String (i + 1),
                        x, gridY - 18, cellSize, 16,
                        juce::Justification::centred);
        }

        // Sidebar divider
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        float splitX = getWidth() * splitRatio;
        g.drawVerticalLine (static_cast<int> (splitX), 45.0f, getHeight() - 10.0f);

        // Sidebar header
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (13.0f);
        juce::String sideTitle = (paramPrefix == "MOD_") ? "MOD ROUTING" : "CARRIER OUTPUTS";
        g.drawText (sideTitle,
                    static_cast<int> (splitX) + 8, 30,
                    getWidth() - static_cast<int> (splitX) - 16, 18,
                    juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromTop (50);

        int totalW = area.getWidth();
        auto gridArea    = area.removeFromLeft (static_cast<int> (totalW * splitRatio));
        auto sidebarArea = area.reduced (8, 4);

        // Place NxN knobs
        int idx = 0;
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
                if (auto* s = matrixSliders[idx++])
                    s->setBounds (gridArea.getX() + gridX + dest * cellSize,
                                  gridY + src * cellSize,
                                  cellSize - 4, cellSize - 2);

        // Sidebar layout
        if (paramPrefix == "MOD_")
        {
            int slotH = sidebarArea.getHeight() / 6;
            for (auto& slot : modSlots)
                slot->setBounds (sidebarArea.removeFromTop (slotH));
        }
        else
        {
            int rowH = sidebarArea.getHeight() / ProjectConfig::numOperators;
            for (int i = 0; i < ProjectConfig::numOperators; ++i)
            {
                auto cell = sidebarArea.removeFromTop (rowH).reduced (0, 2);
                if (auto* lb = outputLabels[i])   lb->setBounds (cell.removeFromLeft (48));
                if (auto* sl = outputSliders[i])  sl->setBounds (cell);
            }
        }
    }

private:
    juce::String paramPrefix;
    juce::String matrixTitle;

    // Grid geometry — tweak these to taste
    static constexpr int gridX    = 40;   // left offset for first column
    static constexpr int gridY    = 60;   // top offset for first row
    static constexpr int cellSize = 90;   // px per cell (knob + label)					  
    //static constexpr float splitRatio = 0.60f;
    static constexpr float splitRatio = 0.65f;


    juce::OwnedArray<juce::Slider>     matrixSliders;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> matrixAttachments;

    std::vector<std::unique_ptr<ModMatrixSlot>> modSlots;

    juce::OwnedArray<juce::Slider>     outputSliders;
    juce::OwnedArray<juce::Label>      outputLabels;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachments;
};
