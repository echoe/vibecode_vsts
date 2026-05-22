#include "PluginProcessor.h"
#include "PluginEditor.h"

OpRowComponent::OpRowComponent(int index, juce::AudioProcessorValueTreeState& state)
    : opIdx(index), apvts(state), prefix("op" + juce::String(index + 1) + "_") 
{
    addAndMakeVisible(waveModeCombo);
    waveModeCombo.addItemList({"Sound Wave", "Additive", "Resonator", "Filter"}, 1);
    waveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, prefix + "wave", waveModeCombo);
    waveModeCombo.addListener(this);

    setupDynamicComponents();

    auto addGlobalSlider = [this](juce::Slider& s, juce::String param, float def) {
        addAndMakeVisible(s);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        globalAttaches.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + param, s));
    };

    addGlobalSlider(attackSl, "attack", 0.1f);
    addGlobalSlider(decaySl, "decay", 0.1f);
    addGlobalSlider(sustainSl, "sustain", 1.0f);
    addGlobalSlider(releaseSl, "release", 0.2f);
    addGlobalSlider(outputSl, "output", 100.0f);

    updateVisibleParameters();
}

void OpRowComponent::setupDynamicComponents() {
    addChildComponent(soundTypeCombo);
    soundTypeCombo.addItemList({"Sine", "Saw", "Pulse", "Square"}, 1);
    comboAttaches.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, prefix + "sound_type", soundTypeCombo));

    auto initSlider = [this](juce::Slider& s, juce::String p) {
        addChildComponent(s);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        sliderAttaches.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + p, s));
    };

    initSlider(ratioSlider, "ratio");
    initSlider(pwSlider, "pw");

    for (int i = 0; i < 8; ++i) {
        initSlider(partialSliders[i], "partial_" + juce::String(i + 1));
        partialSliders[i].setSliderStyle(juce::Slider::LinearBarVertical);
    }

    addChildComponent(resKeyCombo);
    resKeyCombo.addItemList({"A","A#","B","B#","C","C#","D","D#","E","E#","F","F#","G","G#"}, 1);
    comboAttaches.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, prefix + "res_key", resKeyCombo));
    initSlider(qSlider, "res_q");
    initSlider(dampSlider, "res_damping");

    addChildComponent(filtTypeCombo);
    filtTypeCombo.addItemList({"LP12", "LP24", "HP12", "HP24"}, 1);
    comboAttaches.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, prefix + "filt_type", filtTypeCombo));
    initSlider(resSlider, "filt_res");
    initSlider(cutoffSlider, "filt_cutoff");
}

void OpRowComponent::updateVisibleParameters() {
    int mode = waveModeCombo.getSelectedItemIndex();
    
    soundTypeCombo.setVisible(mode == 0);
    ratioSlider.setVisible(mode == 0);
    pwSlider.setVisible(mode == 0);

    for (auto& p : partialSliders) p.setVisible(mode == 1);

    resKeyCombo.setVisible(mode == 2);
    qSlider.setVisible(mode == 2);
    dampSlider.setVisible(mode == 2);

    filtTypeCombo.setVisible(mode == 3);
    resSlider.setVisible(mode == 3);
    cutoffSlider.setVisible(mode == 3);
}

void OpRowComponent::comboBoxChanged(juce::ComboBox*) {
    updateVisibleParameters();
    resized();
}

void OpRowComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::darkgrey.darker(0.3f));
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);
    g.setColour(juce::Colours::white);
    g.drawText(juce::String(opIdx + 1), 5, getHeight()/2 - 10, 20, 20, juce::Justification::centred);
}

void OpRowComponent::resized() {
    auto area = getLocalBounds().withTrimmedLeft(25);
    waveModeCombo.setBounds(area.removeFromLeft(95).reduced(2, 20));
    
    auto dynamicArea = area.removeFromLeft(280).reduced(4, 4);
    int mode = waveModeCombo.getSelectedItemIndex();
    
    if (mode == 0) {
        soundTypeCombo.setBounds(dynamicArea.removeFromLeft(70).reduced(2, 16));
        ratioSlider.setBounds(dynamicArea.removeFromLeft(100).reduced(2, 10));
        pwSlider.setBounds(dynamicArea.removeFromLeft(100).reduced(2, 10));
    } else if (mode == 1) {
        int w = dynamicArea.getWidth() / 8;
        for (int i = 0; i < 8; ++i) partialSliders[i].setBounds(dynamicArea.removeFromLeft(w).reduced(1, 2));
    } else if (mode == 2) {
        resKeyCombo.setBounds(dynamicArea.removeFromLeft(70).reduced(2, 16));
        qSlider.setBounds(dynamicArea.removeFromLeft(100).reduced(2, 10));
        dampSlider.setBounds(dynamicArea.removeFromLeft(100).reduced(2, 10));
    } else if (mode == 3) {
        filtTypeCombo.setBounds(dynamicArea.removeFromLeft(70).reduced(2, 16));
        resSlider.setBounds(dynamicArea.removeFromLeft(100).reduced(2, 10));
        cutoffSlider.setBounds(dynamicArea.removeFromLeft(100).reduced(2, 10));
    }

    auto envArea = area.removeFromLeft(200);
    int envW = envArea.getWidth() / 4;
    attackSl.setBounds(envArea.removeFromLeft(envW).reduced(2));
    decaySl.setBounds(envArea.removeFromLeft(envW).reduced(2));
    sustainSl.setBounds(envArea.removeFromLeft(envW).reduced(2));
    releaseSl.setBounds(envArea.removeFromLeft(envW).reduced(2));
    
    outputSl.setBounds(area.reduced(10, 10));
}

ModulationPage::ModulationPage(juce::AudioProcessorValueTreeState& state) {
    for (int r = 0; r < numOps; ++r) {
        for (int c = 0; c < numOps; ++c) {
            auto& s = matrixSliders[r][c];
            addAndMakeVisible(s);
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            juce::String pId = "fm_" + juce::String(r + 1) + "_" + juce::String(c + 1);
            matrixAttaches.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, pId, s));
        }
    }

    for (int i = 0; i < 2; ++i) {
        juce::String pfx = "lfo" + juce::String(i + 1) + "_";
        auto& lfo = lfoUis[i];
        
        addAndMakeVisible(lfo.target);
        lfo.target.addItemList({"None", "Cutoff", "Pitch", "Volume"}, 1);
        lfo.tarAt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, pfx + "target", lfo.target);
        
        addAndMakeVisible(lfo.type);
        lfo.type.addItemList({"Sine", "Saw", "Pulse", "Square"}, 1);
        lfo.typAt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, pfx + "type", lfo.type);
        
        addAndMakeVisible(lfo.rate);
        lfo.rate.setSliderStyle(juce::Slider::LinearHorizontal);
        lfo.ratAt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, pfx + "rate", lfo.rate);
    }
}

void ModulationPage::resized() {
    auto area = getLocalBounds();
    auto matrixArea = area.removeFromTop(200).reduced(10);
    int cellW = matrixArea.getWidth() / numOps;
    int cellH = matrixArea.getHeight() / numOps;
    
    for (int r = 0; r < numOps; ++r) {
        auto rowArea = matrixArea.removeFromTop(cellH);
        for (int c = 0; c < numOps; ++c) {
            matrixSliders[r][c].setBounds(rowArea.removeFromLeft(cellW).reduced(4));
        }
    }

    auto lfoArea = area.reduced(10);
    int lfoH = lfoArea.getHeight() / 2;
    for (int i = 0; i < 2; ++i) {
        auto row = lfoArea.removeFromTop(lfoH).reduced(0, 5);
        lfoUis[i].target.setBounds(row.removeFromLeft(120).reduced(4, 10));
        lfoUis[i].type.setBounds(row.removeFromLeft(100).reduced(4, 10));
        lfoUis[i].rate.setBounds(row.reduced(4, 10));
    }
}

NewFourOpSynthAudioProcessorEditor::NewFourOpSynthAudioProcessorEditor (NewFourOpSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), scopeComponent(p.scopeBuffer), oscPage(p.apvts), modPage(p.apvts)
{
    addAndMakeVisible(presetLabel);
    presetLabel.setText("Preset: Init", juce::dontSendNotification);
    presetLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    
    addAndMakeVisible(scopeComponent);
    
    addAndMakeVisible(masterVolSlider);
    masterVolSlider.setSliderStyle(juce::Slider::LinearBar);
    masterVolSlider.setRange(0.0, 100.0, 1.0);
    masterVolAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "master_volume", masterVolSlider);

    addAndMakeVisible(oscPageBtn);
    addAndMakeVisible(modPageBtn);
    oscPageBtn.addListener(this);
    modPageBtn.addListener(this);

    addChildComponent(oscPage);
    addChildComponent(modPage);
    
    oscPage.setVisible(true); // Default selection on run

    setSize (700, 520);
    audioProcessor.resetToInitPreset();
}

NewFourOpSynthAudioProcessorEditor::~NewFourOpSynthAudioProcessorEditor() {}

void NewFourOpSynthAudioProcessorEditor::paint (juce::Graphics& g) {
    g.fillAll (juce::Colours::black.brighter(0.1f));
}

void NewFourOpSynthAudioProcessorEditor::resized() {
    auto area = getLocalBounds();
    auto topBar = area.removeFromTop(80).reduced(5);
    
    presetLabel.setBounds(topBar.removeFromLeft(120).reduced(5));
    masterVolSlider.setBounds(topBar.removeFromRight(100).reduced(5, 20));
    scopeComponent.setBounds(topBar.reduced(5));

    auto navBar = area.removeFromTop(40);
    oscPageBtn.setBounds(navBar.removeFromLeft(navBar.getWidth() / 2));
    modPageBtn.setBounds(navBar);

    oscPage.setBounds(area);
    modPage.setBounds(area);
}

void NewFourOpSynthAudioProcessorEditor::buttonClicked(juce::Button* b) {
    oscPage.setVisible(b == &oscPageBtn);
    modPage.setVisible(b == &modPageBtn);
}
