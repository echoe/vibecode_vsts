// PresetBar.h
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PresetBar : public juce::Component
{
public:
    // Targeting flag to isolate target parameters surgically
    enum class RandomTarget
    {
        OperatorsAndEnvelopes,
        ModulationMatrix,
        RoutingMatrix
    };

    PresetBar (FMPluginAudioProcessor& processorToLink)
        : audioProcessor (processorToLink)
    {
        // 1. Configure and style buttons via lambda helper
        auto setupButton = [this] (juce::TextButton& btn, const juce::String& text) {
            btn.setButtonText (text);
            addAndMakeVisible (btn);
        };

        // Standard Utility Presets
        setupButton (initButton, "Init");
        setupButton (saveButton, "Save");
        setupButton (loadButton, "Load");

        // The Split Isolated Randomizers (Kept compact to fit layout boundaries)
        setupButton (randOpsButton, "R-Synth");
        setupButton (randModButton, "R-FM");
        setupButton (randRouteButton, "R-Route");

        // 2. Assign Button Click Actions
        initButton.onClick      = [this] { triggerInit(); };
        saveButton.onClick      = [this] { triggerSave(); };
        loadButton.onClick      = [this] { triggerLoad(); };
        
        // Wire up the targeted randomizers
        randOpsButton.onClick   = [this] { triggerRandomizer (RandomTarget::OperatorsAndEnvelopes); };
        randModButton.onClick   = [this] { triggerRandomizer (RandomTarget::ModulationMatrix); };
        randRouteButton.onClick = [this] { triggerRandomizer (RandomTarget::RoutingMatrix); };
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
        // We now have 6 buttons total to evenly distribute horizontally
        auto area = getLocalBounds().reduced (2);
        int buttonWidth = area.getWidth() / 6;

        initButton.setBounds (area.removeFromLeft (buttonWidth).reduced (1));
        randOpsButton.setBounds (area.removeFromLeft (buttonWidth).reduced (1));
        randModButton.setBounds (area.removeFromLeft (buttonWidth).reduced (1));
        randRouteButton.setBounds (area.removeFromLeft (buttonWidth).reduced (1));
        saveButton.setBounds (area.removeFromLeft (buttonWidth).reduced (1));
        loadButton.setBounds (area.reduced (1)); // Leftover space goes to Load
    }

private:
    void triggerInit()
    {
        auto& parameters = audioProcessor.apvts.processor.getParameters();
        for (auto* param : parameters)
        {
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
                rangedParam->setValueNotifyingHost (rangedParam->getDefaultValue());
        }
    }

    // Surgical Targeted Randomizer Engine
    void triggerRandomizer (RandomTarget target)
    {
        auto& prng = juce::Random::getSystemRandom();
        auto& parameters = audioProcessor.apvts.processor.getParameters();
        
        for (auto* param : parameters)
        {
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
            {
                juce::String paramID = rangedParam->getParameterID();
                
                // CRITICAL SAFETY NET: Absolutely lock out your limiter parameters!
                if (paramID.containsIgnoreCase ("LIMITER")) 
                    continue;

                bool shouldRandomize = false;

                switch (target)
                {
                    case RandomTarget::OperatorsAndEnvelopes:
                        // Randomize if it is NOT a row/column cell in either grid matrix
                        if (!paramID.startsWith ("MOD_") && !paramID.startsWith ("AUDIO_ROUTE_"))
                            shouldRandomize = true;
                        break;

                    case RandomTarget::ModulationMatrix:
                        // Isolate strictly Phase Modulation links
                        if (paramID.startsWith ("MOD_"))
                            shouldRandomize = true;
                        break;

                    case RandomTarget::RoutingMatrix:
                        // Isolate strictly Raw Audio Routing links
                        if (paramID.startsWith ("AUDIO_ROUTE_"))
                            shouldRandomize = true;
                        break;
                }

                if (shouldRandomize)
                {
                    // setValueNotifyingHost takes a normalized float between 0.0f and 1.0f
                    float randomValue = prng.nextFloat();
                    rangedParam->setValueNotifyingHost (randomValue);
                }
            }
        }
    }

    void triggerSave()
    {
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

                if (xml != nullptr && xml->hasTagName (audioProcessor.apvts.state.getType()))
                {
                    audioProcessor.apvts.replaceState (juce::ValueTree::fromXml (*xml));
                }
            }
        });
    }

    FMPluginAudioProcessor& audioProcessor;
    
    // UI Elements Group
    juce::TextButton initButton, saveButton, loadButton;
    juce::TextButton randOpsButton, randModButton, randRouteButton;

    std::unique_ptr<juce::FileChooser> fileChooser;
};
