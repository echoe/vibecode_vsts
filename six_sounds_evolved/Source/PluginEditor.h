#pragma once
#include "PluginProcessor.h"
#include "OperatorsPage.h"
#include "MatrixPage.h"
#include "PresetBar.h"

class FMPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    enum class PageView { Operators, Matrix };

    // Simply declare the constructor and destructor here. No curly braces!
    FMPluginAudioProcessorEditor (FMPluginAudioProcessor&);
    ~FMPluginAudioProcessorEditor() override;

    void setPage (PageView pageToDisplay);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    FMPluginAudioProcessor& audioProcessor;

    PresetBar presetBar; // <-- Preset support
    juce::TextButton opsPageButton, matrixPageButton, filtersPageButton;
    OperatorsPage opsPage;
    MatrixPage matrixPage;

    PageView currentPage = PageView::Operators;
    juce::Slider limiterCeilSlider;
    juce::Label limiterCeilLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> limiterCeilAttach;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FMPluginAudioProcessorEditor)
};
