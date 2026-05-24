#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PresetBar : public juce::Component
{
public:
    PresetBar (FMPluginAudioProcessor& processorToLink)
        : audioProcessor (processorToLink)
    {
        // 1. Configure and style buttons
        auto setupButton = [this] (juce::TextButton& btn, const juce::String& text) {
            btn.setButtonText (text);
            addAndMakeVisible (btn);
        };

        setupButton (initButton, "Init Preset");
        setupButton (randomButton, "Randomize");
        setupButton (saveButton, "Save Preset");
        setupButton (loadButton, "Load Preset");

        // 2. Assign Button Click Actions
        initButton.onClick   = [this] { triggerInit(); };
        randomButton.onClick = [this] { triggerRandomizer(); };
        saveButton.onClick   = [this] { triggerSave(); };
        loadButton.onClick   = [this] { triggerLoad(); };
    }

    void paint (juce::Graphics& g) override
    {
        // Draw a dark utility background with a subtle separation line at the bottom
        g.fillAll (juce::Colours::black.brighter (0.1f));
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (5);
        int buttonWidth = area.getWidth() / 4;

        initButton.setBounds (area.removeFromLeft (buttonWidth).reduced (2));
        randomButton.setBounds (area.removeFromLeft (buttonWidth).reduced (2));
        saveButton.setBounds (area.removeFromLeft (buttonWidth).reduced (2));
        loadButton.setBounds (area.reduced (2)); // Leftover space goes to Load
    }

private:
    void triggerInit()
    {
        // Loop through all parameters and reset them to their default normalized values (0.0 to 1.0)
        auto& parameters = audioProcessor.apvts.processor.getParameters();
        for (auto* param : parameters)
        {
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
                rangedParam->setValueNotifyingHost (rangedParam->getDefaultValue());
        }
    }

    void triggerRandomizer()
    {
        auto& prng = juce::Random::getSystemRandom();
        auto& parameters = audioProcessor.apvts.processor.getParameters();

        for (auto* param : parameters)
        {
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
            {
                // setValueNotifyingHost takes a normalized float between 0.0f and 1.0f
                float randomValue = prng.nextFloat();
                rangedParam->setValueNotifyingHost (randomValue);
            }
        }
    }

    void triggerSave()
    {
        // Modern JUCE async file browser initialization
        fileChooser = std::make_unique<juce::FileChooser> (
            "Save Synth Preset",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.xml"
        );

        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file != juce::File())
            {
                // Capture current APVTS ValueTree layout state, convert to XML text, and dump to file
                auto state = audioProcessor.apvts.copyState();
                std::unique_ptr<juce::XmlElement> xml (state.createXml());
                xml->writeTo (file);
            }
        });
    }

    void triggerLoad()
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load Synth Preset",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.xml"
        );

        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file.existsAsFile())
            {
                std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));
                
                // Safety check to verify XML root element matches parameter layout context tags
                if (xml != nullptr && xml->hasTagName (audioProcessor.apvts.state.getType()))
                {
                    audioProcessor.apvts.replaceState (juce::ValueTree::fromXml (*xml));
                }
            }
        });
    }

    FMPluginAudioProcessor& audioProcessor;
    juce::TextButton initButton, randomButton, saveButton, loadButton;
    
    // Kept as a persistent unique_ptr to prevent going out of scope during async execution threads
    std::unique_ptr<juce::FileChooser> fileChooser;
};
