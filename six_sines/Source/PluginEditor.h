#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MatrixFMSynthAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    MatrixFMSynthAudioProcessorEditor (MatrixFMSynthAudioProcessor&);
    ~MatrixFMSynthAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void updateScale();
private:
    MatrixFMSynthAudioProcessor& audioProcessor;
    struct ModRow {
        juce::ComboBox source, dest; juce::Slider amount;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAtt, destAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAtt;
    };
    juce::GroupComponent envGroup, lfoGroup, matrixGroup, toolsGroup, masterGroup, modGroup;
    juce::Label adsrTitle1, adsrTitle2, visualizerLabel, masterLabel, lfoLabels[2];
    // In Private section of MatrixFMSynthAudioProcessorEditor
    juce::Label envLabels[8]; // A, D, S, R for both envs
    juce::Label lfoParamLabels[6]; // Rate, Amt, Depth for both LFOs
    juce::Label opHeaderLabels[9]; // For: Gain, Env, Ratio, Mode, Context, Context2, Track, Matrix, Out
    juce::Label opRowNumbers[6];   // For: OP 1, OP 2, etc.
    juce::Slider masterSlider; juce::TextButton saveBtn{"SAVE"}, loadBtn{"LOAD"}, initBtn{"INIT"}, randModBtn{"RAND MOD"};
    juce::ComboBox uiScaleMenu; juce::ResizableCornerComponent resizer;
    juce::Slider adsr1Sliders[4], adsr2Sliders[4], lfoFreq[2], lfoAmt[2], lfoDirectDepth[2];
    juce::ComboBox lfoWave[2], lfoDirectDest[2]; juce::ToggleButton lfoSyncBtn[2], envRetrigBtn[2];
    juce::OwnedArray<ModRow> modRows;
    juce::OwnedArray<juce::Slider> matrixKnobs, opGains, opGainsOut, opRatios, opContext1, opContext2, opTrack;
    juce::OwnedArray<juce::ComboBox> opModes, opEnvSelect;
    juce::OwnedArray<juce::Label> rowLabels;
    juce::OwnedArray<juce::Label> opContextLabels1; // Labels for C1
    juce::OwnedArray<juce::Label> opContextLabels2; // Labels for C2
    juce::OwnedArray<juce::Label> opTrackLabels;   // Labels for Keytrack
    juce::OwnedArray<juce::Label> matrixValueLabels;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAtt;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sAtts;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtts;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtts;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MatrixFMSynthAudioProcessorEditor)
};
