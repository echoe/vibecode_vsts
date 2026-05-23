#include "PluginProcessor.h"
#include "PluginEditor.h"

FMPluginAudioProcessorEditor::FMPluginAudioProcessorEditor (FMPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), 
      audioProcessor (p),
      presetBar (p), //initialize preset bar	
      opsPage (p.apvts),     // Pass APVTS context straight down
      matrixPage (p.apvts)   // Pass APVTS context straight down
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (opsPage);
    addAndMakeVisible (matrixPage);

    // Navigation Buttons Configuration
    addAndMakeVisible (opsPageButton);
    opsPageButton.setButtonText ("Operators & Envelopes");
    opsPageButton.onClick = [this] { setPage (PageView::Operators); };

    addAndMakeVisible (matrixPageButton);
    matrixPageButton.setButtonText ("Modulation Matrix");
    matrixPageButton.onClick = [this] { setPage (PageView::Matrix); };

    setPage (PageView::Operators);
    setSize (650, 380); // Expanded boundary footprint to comfortably show labels
}

FMPluginAudioProcessorEditor::~FMPluginAudioProcessorEditor() {}

void FMPluginAudioProcessorEditor::setPage (PageView pageToDisplay)
{
    currentPage = pageToDisplay;
    opsPage.setVisible (currentPage == PageView::Operators);
    matrixPage.setVisible (currentPage == PageView::Matrix);
}

void FMPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.9f));
}

void FMPluginAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    
    // 1. Dedicate the absolute top 40px block to Preset Management controls
    presetBar.setBounds (area.removeFromTop (40));
    
    // 2. Dedicate the subsequent 40px block underneath to UI Navigation Page switching
    auto navArea = area.removeFromTop (40);
    opsPageButton.setBounds (navArea.removeFromLeft (getWidth() / 2).reduced (4));
    matrixPageButton.setBounds (navArea.reduced (4));
    // The active page occupies the remaining container bounds
    opsPage.setBounds (area);
    matrixPage.setBounds (area);
}
