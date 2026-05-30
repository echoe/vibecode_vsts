// PluginEditor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

FMPluginAudioProcessorEditor::FMPluginAudioProcessorEditor (FMPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), 
      audioProcessor (p),
      presetBar (p), //initialize preset bar	
      opsPage (p.apvts),     // Pass APVTS context straight down
      matrixPage (p.apvts, "MOD_", "Modulation Matrix"),   // Pass APVTS context straight down
      audioMatrixPage (p.apvts, "AUDIO_ROUTE_", "Routing Matrix"), // APVTS context etc.
      effectsPage (p.apvts) // APVTS context etc.
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (opsPage);
    addAndMakeVisible (matrixPage);
    addAndMakeVisible (audioMatrixPage);
    addAndMakeVisible (effectsPage);

    // Navigation Buttons Configuration
    addAndMakeVisible (opsPageButton);
    opsPageButton.setButtonText ("Operators & Envelopes");
    opsPageButton.onClick = [this] { setPage (PageView::Operators); };

    addAndMakeVisible (matrixPageButton);
    matrixPageButton.setButtonText ("Modulation Matrix");
    matrixPageButton.onClick = [this] { setPage (PageView::Matrix); };

    addAndMakeVisible (audioMatrixPageButton);
    audioMatrixPageButton.setButtonText ("Routing Matrix");
    audioMatrixPageButton.onClick = [this] { setPage (PageView::AudioMatrix); };

    addAndMakeVisible (effectsPageButton);
    effectsPageButton.setButtonText ("Effects");
    effectsPageButton.onClick = [this] { setPage (PageView::Effects); };

    // Gain slider, since a JUCE limiter simply does not work
    // Configure the Slider style
    gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 45, 18);
    addAndMakeVisible (gainSlider);

    // Add a text label
    gainLabel.setText ("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (gainLabel);

    // Secure the attachment bridge
    gainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        p.apvts, "GAIN_CEIL", gainSlider);

    // Add title
    titleLabel.setText ("Only Oscs", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::Font (juce::FontOptions().withHeight (18.0f).withStyle ("Bold")));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    setPage (PageView::Operators);
    setSize (950, 680); // Expanded boundary footprint to comfortably show labels
}

FMPluginAudioProcessorEditor::~FMPluginAudioProcessorEditor() {}

void FMPluginAudioProcessorEditor::setPage (PageView pageToDisplay)
{
    currentPage = pageToDisplay;
    // Toggle visibility based on the active page enum state
    opsPage.setVisible (currentPage == PageView::Operators);
    matrixPage.setVisible (currentPage == PageView::Matrix);
    audioMatrixPage.setVisible (currentPage == PageView::AudioMatrix);
    effectsPage.setVisible (currentPage == PageView::Effects);
    // Ensure the top button highlight states visually match the current selection
    opsPageButton.setToggleState (currentPage == PageView::Operators, juce::dontSendNotification);
    matrixPageButton.setToggleState (currentPage == PageView::Matrix, juce::dontSendNotification);
    audioMatrixPageButton.setToggleState (currentPage == PageView::AudioMatrix, juce::dontSendNotification);
    effectsPageButton.setToggleState (currentPage == PageView::Effects, juce::dontSendNotification);
}

void FMPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.9f));
}

void FMPluginAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    // 1. Dedicate the absolute top 40px block to Preset Management controls
    auto topBarArea = area.removeFromTop (40);
    // Assuming your preset area takes up the left chunk:
    auto presetArea = topBarArea.removeFromLeft(300);
    presetBar.setBounds (presetArea.reduced (2));
    // 2. Head to the absolute right side of the bar to build the safety valve
    auto gainArea = topBarArea.removeFromRight (220);
    gainLabel.setBounds (gainArea.removeFromLeft (45));
    gainSlider.setBounds (gainArea.reduced (2));

    // Add title in remaining area in top 
    titleLabel.setBounds (topBarArea.reduced (2));

    // 2. Dedicate the subsequent 40px block underneath to UI Navigation Page switching
    auto navArea = area.removeFromTop (40);
    int buttonWidth = getWidth() / 4;
    opsPageButton.setBounds (navArea.removeFromLeft (buttonWidth).reduced (4));
    matrixPageButton.setBounds (navArea.removeFromLeft (buttonWidth).reduced (4));
    audioMatrixPageButton.setBounds (navArea.removeFromLeft (buttonWidth).reduced (4));
    effectsPageButton.setBounds (navArea.reduced (4));
    // The active page occupies the remaining container bounds
    opsPage.setBounds (area);
    matrixPage.setBounds (area);
    audioMatrixPage.setBounds (area);
    effectsPage.setBounds (area);
}
