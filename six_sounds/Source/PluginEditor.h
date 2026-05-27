// PluginEditor.h
#pragma once
#include "PluginProcessor.h"
#include "OperatorsPage.h"
#include "MatrixPage.h"
#include "PresetBar.h"
#include "EffectsPage.h"

class FMPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    enum class PageView { Operators, Matrix, AudioMatrix, Effects };

    // Simply declare the constructor and destructor here. No curly braces!
    FMPluginAudioProcessorEditor (FMPluginAudioProcessor&);
    ~FMPluginAudioProcessorEditor() override;

    void setPage (PageView pageToDisplay);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    FMPluginAudioProcessor& audioProcessor;
    PresetBar presetBar; // <-- Preset support
    juce::TextButton opsPageButton, matrixPageButton, audioMatrixPageButton, effectsPageButton;
    OperatorsPage opsPage;
    MatrixPage matrixPage;
    MatrixPage audioMatrixPage;
    EffectsPage effectsPage;

    PageView currentPage = PageView::Operators;
    juce::Slider gainSlider;
    juce::Label gainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttach;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FMPluginAudioProcessorEditor)
};
