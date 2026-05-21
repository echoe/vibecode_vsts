#include "PluginProcessor.h"

SpectralResonatorAudioProcessor::SpectralResonatorAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      mState (*this, nullptr, "PARAMETERS", {
          std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"freq", 1}, "Frequency", juce::NormalisableRange<float>(60.0f, 4000.0f, 0.0f, 0.35f), 440.0f),
          std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"res", 1}, "Resonance", 0.0f, 1.0f, 0.5f),
          std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"decay", 1}, "Decay Time", 0.0f, 0.99f, 0.85f)
      })
{}

void SpectralResonatorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    auto totalNumInputChannels = getTotalNumInputChannels();
    mEngines.resize (totalNumInputChannels);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        mEngines[channel].prepare (sampleRate);
    }
}

bool SpectralResonatorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // In JUCE 8, use getChannelSet(isInput, busIndex) to query layout configurations
    auto mainInputLayout  = layouts.getChannelSet (true, 0);
    auto mainOutputLayout = layouts.getChannelSet (false, 0);

    // Only allow mono or stereo configurations
    if (mainOutputLayout != juce::AudioChannelSet::mono()
     && mainOutputLayout != juce::AudioChannelSet::stereo())
        return false;

    // Ensure the input channel configuration matches the output configuration
    if (mainOutputLayout != mainInputLayout)
        return false;

    return true;
}

void SpectralResonatorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Fetch snapshot values of current parameters safely from thread cache
    float targetFreq = mState.getRawParameterValue ("freq")->load();
    float resonance  = mState.getRawParameterValue ("res")->load();
    float decay      = mState.getRawParameterValue ("decay")->load();

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        mEngines[channel].processBlock (channelData, buffer.getNumSamples(), targetFreq, resonance, decay);
    }
}

void SpectralResonatorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = mState.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SpectralResonatorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (mState.state.getType()))
        mState.replaceState (juce::ValueTree::fromXml (*xmlState));
}

// Global factory hook instantiation
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectralResonatorAudioProcessor();
}
