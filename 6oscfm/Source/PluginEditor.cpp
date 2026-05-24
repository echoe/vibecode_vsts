#include "PluginProcessor.h"
#include "PluginEditor.h"

FMPluginAudioProcessorEditor::FMPluginAudioProcessorEditor (FMPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), 
      audioProcessor (p),
      presetBar (p), //initialize preset bar	
      opsPage (p.apvts),     // Pass APVTS context straight down
      filtersPage (p.apvts),
      matrixPage (p.apvts)   // Pass APVTS context straight down
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (opsPage);
    addAndMakeVisible (filtersPage);
    addAndMakeVisible (matrixPage);

    // Navigation Buttons Configuration
    addAndMakeVisible (opsPageButton);
    opsPageButton.setButtonText ("Operators & Envelopes");
    opsPageButton.onClick = [this] { setPage (PageView::Operators); };

    addAndMakeVisible (matrixPageButton);
    matrixPageButton.setButtonText ("Modulation Matrix");
    matrixPageButton.onClick = [this] { setPage (PageView::Matrix); };

    addAndMakeVisible (filtersPageButton);
    filtersPageButton.setButtonText ("Filters");
    filtersPageButton.onClick = [this] { setPage (PageView::Filters); };

    // Limiter slider:
    // Configure the Slider style
    limiterCeilSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    limiterCeilSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 45, 18);
    addAndMakeVisible (limiterCeilSlider);

    // Add a text label
    limiterCeilLabel.setText ("Limit", juce::dontSendNotification);
    limiterCeilLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (limiterCeilLabel);

    // Secure the attachment bridge
    limiterCeilAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        p.apvts, "LIMITER_CEIL", limiterCeilSlider);

    setPage (PageView::Operators);
    setSize (950, 680); // Expanded boundary footprint to comfortably show labels
}

FMPluginAudioProcessorEditor::~FMPluginAudioProcessorEditor() {}

void FMPluginAudioProcessorEditor::setPage (PageView pageToDisplay)
{
    currentPage = pageToDisplay;
    opsPage.setVisible (currentPage == PageView::Operators);
    filtersPage.setVisible (currentPage == PageView::Filters);
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
    auto topBarArea = area.removeFromTop (40);
    
    // Assuming your preset area takes up the left chunk:
    auto presetArea = topBarArea.removeFromLeft(300);
    presetBar.setBounds (presetArea.reduced (2));

    // 2. Head to the absolute right side of the bar to build the safety valve
    auto limiterArea = topBarArea.removeFromRight (220); 
    limiterCeilLabel.setBounds (limiterArea.removeFromLeft (45));
    limiterCeilSlider.setBounds (limiterArea.reduced (2));

    // 2. Dedicate the subsequent 40px block underneath to UI Navigation Page switching
    auto navArea = area.removeFromTop (40);
    int buttonWidth = getWidth() / 3;
    opsPageButton.setBounds (navArea.removeFromLeft (buttonWidth).reduced (4));
    filtersPageButton.setBounds (navArea.removeFromLeft (buttonWidth).reduced (4));
    matrixPageButton.setBounds (navArea.reduced (4));
    // The active page occupies the remaining container bounds
    opsPage.setBounds (area);
    filtersPage.setBounds (area);
    matrixPage.setBounds (area);
}
