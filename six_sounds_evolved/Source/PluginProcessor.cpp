#include "PluginProcessor.h"
#include "PluginEditor.h"

NewFourOpSynthAudioProcessor::NewFourOpSynthAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    synth.clearVoices();
    for (int i = 0; i < 8; ++i) { synth.addVoice(new SynthVoice()); }
    synth.clearSounds();
    synth.addSound(new SynthSound());
}

NewFourOpSynthAudioProcessor::~NewFourOpSynthAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout NewFourOpSynthAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("master_volume", "Master Vol", 0.0f, 100.0f, 80.0f));

    for (int i = 0; i < numOps; ++i) {
        juce::String prefix = "op" + juce::String(i + 1) + "_";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(prefix + "wave", "Wave Mode", juce::StringArray{"Sound Wave","Additive","Resonator","Filter"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(prefix + "sound_type", "Type", juce::StringArray{"Sine","Saw","Pulse","Square"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(prefix + "ratio", "Ratio", 0.0f, 16.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(prefix + "pw", "Pulsewidth", 0.0f, 100.0f, 50.0f));
        
        for (int p = 0; p < 8; ++p) {
            params.push_back(std::make_unique<juce::AudioParameterInt>(prefix + "partial_" + juce::String(p + 1), "Partial", 1, 8, 1));
        }
        
        params.push_back(std::make_unique<juce::AudioParameterChoice>(prefix + "res_key", "Key", juce::StringArray{"A","A#","B","B#","C","C#","D","D#","E","E#","F","F#","G","G#"}, 4));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(prefix + "res_q", "Q", 0.0f, 100.0f, 50.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(prefix + "res_damping", "Damping", 0.0f, 100.0f, 50.0f));
        
        params.push_back(std::make_unique<juce::AudioParameterChoice>(prefix + "filt_type", "Filter Type", juce::StringArray{"LP12","LP24","HP12","HP24"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(prefix + "filt_res", "Resonance", 0.0f, 100.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(prefix + "filt_cutoff", "Cutoff", 0.0f, 100.0f, 100.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(prefix + "attack", "Attack", 0.0f, 5.0f, 0.1f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(prefix + "decay", "Decay", 0.0f, 5.0f, 0.1f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(prefix + "sustain", "Sustain", 0.0f, 1.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(prefix + "release", "Release", 0.0f, 5.0f, 0.2f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(prefix + "output", "Output Level", 0.0f, 100.0f, 100.0f));
    }

    for (int r = 0; r < numOps; ++r) {
        for (int c = 0; c < numOps; ++c) {
            params.push_back(std::make_unique<juce::AudioParameterFloat>("fm_" + juce::String(r+1) + "_" + juce::String(c+1), "Matrix Point", 0.0f, 100.0f, 0.0f));
        }
    }

    for (int l = 1; l <= 2; ++l) {
        juce::String lPrefix = "lfo" + juce::String(l) + "_";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(lPrefix + "target", "LFO Target", juce::StringArray{"None","Cutoff","Pitch","Volume"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(lPrefix + "rate", "LFO Rate", 0.1f, 20.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(lPrefix + "type", "LFO Type", juce::StringArray{"Sine","Saw","Pulse","Square"}, 0));
    }

    return { params.begin(), params.end() };
}

void NewFourOpSynthAudioProcessor::resetToInitPreset() {
    auto masterVolParam = apvts.getParameter("master_volume");
    if (masterVolParam) masterVolParam->setValueNotifyingHost(masterVolParam->convertTo0to1(80.0f));

    for (int i = 0; i < numOps; ++i) {
        juce::String prefix = "op" + juce::String(i + 1) + "_";
        apvts.getParameter(prefix + "wave")->setValueNotifyingHost(0.0f); // Default: Sound Wave
        apvts.getParameter(prefix + "sound_type")->setValueNotifyingHost(0.0f); // Default: Sine
        apvts.getParameter(prefix + "ratio")->setValueNotifyingHost(apvts.getParameter(prefix + "ratio")->convertTo0to1(1.0f));
        apvts.getParameter(prefix + "output")->setValueNotifyingHost(apvts.getParameter(prefix + "output")->convertTo0to1(i == 0 ? 100.0f : 0.0f)); // Only Op 1 audible initially
    }
}

void NewFourOpSynthAudioProcessor::updateSynthParameters() {
    for (int v = 0; v < synth.getNumVoices(); ++v) {
        if (auto* voice = dynamic_cast<SynthVoice*>(synth.getVoice(v))) {
            for (int i = 0; i < numOps; ++i) {
                juce::String prefix = "op" + juce::String(i + 1) + "_";
                auto& op = voice->operators[i];
                
                op.waveSel = static_cast<int>(*apvts.getRawParameterValue(prefix + "wave")) + 1;
                op.soundType = static_cast<int>(*apvts.getRawParameterValue(prefix + "sound_type")) + 1;
                op.ratio = *apvts.getRawParameterValue(prefix + "ratio");
                op.pulseWidth = *apvts.getRawParameterValue(prefix + "pw") / 100.0f;
                
                for (int p = 0; p < 8; ++p) {
                    op.partials[p] = static_cast<int>(*apvts.getRawParameterValue(prefix + "partial_" + juce::String(p + 1)));
                }
                
                op.resKey = static_cast<int>(*apvts.getRawParameterValue(prefix + "res_key"));
                op.resQ = *apvts.getRawParameterValue(prefix + "res_q");
                op.resDamping = *apvts.getRawParameterValue(prefix + "res_damping");
                op.filtType = static_cast<int>(*apvts.getRawParameterValue(prefix + "filt_type")) + 1;
                op.filtRes = *apvts.getRawParameterValue(prefix + "filt_res");
                op.filtCutoff = *apvts.getRawParameterValue(prefix + "filt_cutoff");
                op.outputLevel = *apvts.getRawParameterValue(prefix + "output") / 100.0f;

                juce::ADSR::Parameters adsrParams;
                adsrParams.attack = *apvts.getRawParameterValue(prefix + "attack");
                adsrParams.decay = *apvts.getRawParameterValue(prefix + "decay");
                adsrParams.sustain = *apvts.getRawParameterValue(prefix + "sustain");
                adsrParams.release = *apvts.getRawParameterValue(prefix + "release");
                op.adsr.setParameters(adsrParams);
            }

            for (int r = 0; r < numOps; ++r) {
                for (int c = 0; c < numOps; ++c) {
                    voice->fmMatrix[r][c] = *apvts.getRawParameterValue("fm_" + juce::String(r+1) + "_" + juce::String(c+1));
                }
            }
        }
    }
}

void NewFourOpSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {
    synth.setCurrentPlaybackSampleRate(sampleRate);
    updateSynthParameters();
}

void NewFourOpSynthAudioProcessor::releaseResources() {}

void NewFourOpSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    updateSynthParameters();
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    float vol = *apvts.getRawParameterValue("master_volume") / 100.0f;
    buffer.applyGain(vol);

    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        scopeBuffer.pushSample(buffer.getSample(0, i));
    }
}

juce::AudioProcessorEditor* NewFourOpSynthAudioProcessor::createEditor() { return new NewFourOpSynthAudioProcessorEditor (*this); }
void NewFourOpSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}
void NewFourOpSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NewFourOpSynthAudioProcessor();
}
