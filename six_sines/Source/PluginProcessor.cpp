#include "PluginProcessor.h"
#include "PluginEditor.h"

MatrixFMSynthAudioProcessor::MatrixFMSynthAudioProcessor() : apvts (*this, nullptr, "Params", createParameterLayout()) {
    synth.addSound(new FMSound()); for (int i=0; i<8; ++i) synth.addVoice(new FMVoice()); visualizer.startTimerHz(30);
}
MatrixFMSynthAudioProcessor::~MatrixFMSynthAudioProcessor() {}

juce::StringArray MatrixFMSynthAudioProcessor::getModDestinations() {
    juce::StringArray d; d.add("None");
    for(int i=0; i<6; ++i) d.add("Op" + juce::String(i) + " Gain");
    for(int i=0; i<6; ++i) d.add("Op" + juce::String(i) + " Ratio");
    for(int i=0; i<6; ++i) d.add("Op" + juce::String(i) + " C1");
    for(int i=0; i<6; ++i) for(int j=0; j<6; ++j) d.add("Mod " + juce::String(j) + "->" + juce::String(i));
    return d;
}

juce::AudioProcessorValueTreeState::ParameterLayout MatrixFMSynthAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>("attack", "A1 Attack", 0.001f, 5.0f, 0.1f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("decay", "A1 Decay", 0.001f, 5.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("sustain", "A1 Sustain", 0.0f, 1.0f, 0.7f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("release", "A1 Release", 0.001f, 5.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>("env1ret", "Env 1 Retrig", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>("a2_attack", "A2 Attack", 0.001f, 5.0f, 0.1f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("a2_decay", "A2 Decay", 0.001f, 5.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("a2_sustain", "A2 Sustain", 0.0f, 1.0f, 0.7f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("a2_release", "A2 Release", 0.001f, 5.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>("env2ret", "Env 2 Retrig", true));

    auto dests = getModDestinations();
    for (int i=0; i<2; ++i) {
        juce::String id = "lfo" + juce::String(i);
        layout.add(std::make_unique<juce::AudioParameterFloat>(id+"freq", "LFO Freq", 0.1f, 30.0f, 1.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(id+"amt", "LFO Amt", 0.0f, 1.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterInt>(id+"wave", "LFO Wave", 0, 3, 0));
        layout.add(std::make_unique<juce::AudioParameterBool>(id+"sync", "LFO Sync", true));
        layout.add(std::make_unique<juce::AudioParameterInt>(id+"directDest", "LFO Dest", 0, dests.size()-1, 0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(id+"directDepth", "LFO Depth", -1.0f, 1.0f, 0.0f));
    }
    for (int i=0; i<8; ++i) {
        juce::String id = "slot" + juce::String(i);
        layout.add(std::make_unique<juce::AudioParameterInt>(id+"src", "Src", 0, 2, 0));
        layout.add(std::make_unique<juce::AudioParameterInt>(id+"dest", "Dest", 0, dests.size()-1, 0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(id+"amt", "Amt", -1.0f, 1.0f, 0.0f));
    }
    for (int i=0; i<6; ++i) {
        juce::String op = "op"+juce::String(i);
        layout.add(std::make_unique<juce::AudioParameterFloat>(op+"gain", "Vol", 0.0f, 1.0f, i==0?0.7f:0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(op+"gainOut", "Out", 0.0f, 1.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(op+"ratio", "Ratio", 0.5f, 20.0f, 1.0f));
        layout.add(std::make_unique<juce::AudioParameterInt>(op+"mode", "Mode", 0, 2, 0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(op+"res", "C1", 0.0f, 1.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(op+"res2", "C2", 0.0f, 1.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(op+"kbtrack", "Track", 0.0f, 1.0f, 0.5f));
        layout.add(std::make_unique<juce::AudioParameterInt>(op+"envSrc", "Env", 0, 1, 0));
        for (int j=0; j<6; ++j) layout.add(std::make_unique<juce::AudioParameterFloat>("m"+juce::String(i)+juce::String(j), "Mod", 0.0f, 1.0f, 0.0f));
    }
    layout.add(std::make_unique<juce::AudioParameterFloat>("master_out", "Master", 0.0f, 1.0f, 0.8f));
    return layout;
}

void MatrixFMSynthAudioProcessor::prepareToPlay (double sr, int s) {
    synth.setCurrentPlaybackSampleRate(sr);
    for (int i=0; i<synth.getNumVoices(); ++i) if (auto v = dynamic_cast<FMVoice*>(synth.getVoice(i))) v->prepare(sr, s);
}

void MatrixFMSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    VoiceParams vp;
    vp.adsr1[0] = *apvts.getRawParameterValue("attack"); vp.adsr1[1] = *apvts.getRawParameterValue("decay");
    vp.adsr1[2] = *apvts.getRawParameterValue("sustain"); vp.adsr1[3] = *apvts.getRawParameterValue("release");
    vp.envRetrigger[0] = (bool)*apvts.getRawParameterValue("env1ret");
    vp.adsr2[0] = *apvts.getRawParameterValue("a2_attack"); vp.adsr2[1] = *apvts.getRawParameterValue("a2_decay");
    vp.adsr2[2] = *apvts.getRawParameterValue("a2_sustain"); vp.adsr2[3] = *apvts.getRawParameterValue("a2_release");
    vp.envRetrigger[1] = (bool)*apvts.getRawParameterValue("env2ret");
    for(int i=0; i<2; ++i) {
        juce::String id = "lfo"+juce::String(i);
        vp.lfos[i].freq = *apvts.getRawParameterValue(id+"freq"); vp.lfos[i].amount = *apvts.getRawParameterValue(id+"amt");
        vp.lfos[i].wave = (int)*apvts.getRawParameterValue(id+"wave"); vp.lfos[i].sync = (bool)*apvts.getRawParameterValue(id+"sync");
        vp.lfoDirectDest[i] = (int)*apvts.getRawParameterValue(id+"directDest"); vp.lfoDirectDepth[i] = *apvts.getRawParameterValue(id+"directDepth");
    }
    for(int i=0; i<8; ++i) {
        juce::String id = "slot"+juce::String(i);
        vp.modSource[i] = (int)*apvts.getRawParameterValue(id+"src"); vp.modDest[i] = (int)*apvts.getRawParameterValue(id+"dest"); vp.modAmount[i] = *apvts.getRawParameterValue(id+"amt");
    }
    for(int i=0; i<6; ++i) {
        juce::String op = "op"+juce::String(i);
        vp.modes[i] = (int)*apvts.getRawParameterValue(op+"mode"); vp.ratios[i] = *apvts.getRawParameterValue(op+"ratio");
        vp.gains[i] = *apvts.getRawParameterValue(op+"gain"); vp.gainsOut[i] = *apvts.getRawParameterValue(op+"gainOut");
        vp.res[i] = *apvts.getRawParameterValue(op+"res"); vp.res2[i] = *apvts.getRawParameterValue(op+"res2");
        vp.kbTrack[i] = *apvts.getRawParameterValue(op+"kbtrack"); vp.opAdsrSource[i] = (int)*apvts.getRawParameterValue(op+"envSrc");
        for(int j=0; j<6; ++j) vp.matrix[i][j] = *apvts.getRawParameterValue("m"+juce::String(i)+juce::String(j));
    }
    for (int i=0; i<synth.getNumVoices(); ++i) if (auto v = dynamic_cast<FMVoice*>(synth.getVoice(i))) v->setCurrentParams(vp);
    buffer.clear(); synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples()); visualizer.pushBuffer(buffer);
}

void MatrixFMSynthAudioProcessor::savePreset() {
    fileChooser = std::make_unique<juce::FileChooser>("Save", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.xml");
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode, [this](const juce::FileChooser& fc){
        auto f = fc.getResult(); if (f != juce::File()) { auto s = apvts.copyState(); std::unique_ptr<juce::XmlElement> x(s.createXml()); x->writeTo(f); }
    });
}
void MatrixFMSynthAudioProcessor::loadPreset() {
    fileChooser = std::make_unique<juce::FileChooser>("Load", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.xml");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode, [this](const juce::FileChooser& fc){
        auto f = fc.getResult(); if (f.exists()) { std::unique_ptr<juce::XmlElement> x(juce::XmlDocument::parse(f)); if(x) apvts.replaceState(juce::ValueTree::fromXml(*x)); }
    });
}
void MatrixFMSynthAudioProcessor::initPreset() { // This is your INIT function
    // Access the parameters directly from the processor
    auto& params = getParameters();
    
    for (auto* param : params) {
        // JUCE parameters are already AudioProcessorParameter pointers
        // so we can set them to their default value directly.
        param->setValueNotifyingHost(param->getDefaultValue());
    }
}
void MatrixFMSynthAudioProcessor::randomizeModMatrix() {
    auto& r = juce::Random::getSystemRandom(); auto ds = getModDestinations();
    for (int i=0; i<8; ++i) {
        juce::String id = "slot"+juce::String(i);
        apvts.getParameter(id+"src")->setValueNotifyingHost(r.nextInt(3)/2.0f);
        apvts.getParameter(id+"dest")->setValueNotifyingHost(r.nextInt(ds.size()-1)/(float)(ds.size()-1));
        apvts.getParameter(id+"amt")->setValueNotifyingHost(r.nextFloat());
    }
}
void MatrixFMSynthAudioProcessor::getStateInformation(juce::MemoryBlock& d) { auto s = apvts.copyState(); std::unique_ptr<juce::XmlElement> x(s.createXml()); copyXmlToBinary(*x, d); }
void MatrixFMSynthAudioProcessor::setStateInformation(const void* d, int s) { std::unique_ptr<juce::XmlElement> x(getXmlFromBinary(d, s)); if(x) apvts.replaceState(juce::ValueTree::fromXml(*x)); }
juce::AudioProcessorEditor* MatrixFMSynthAudioProcessor::createEditor() { return new MatrixFMSynthAudioProcessorEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MatrixFMSynthAudioProcessor(); }
