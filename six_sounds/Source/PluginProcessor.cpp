// PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Constants.h"

FMPluginAudioProcessor::FMPluginAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // The curly braces are critical here so 'voice' exists in scope!
    // This is the number of voices the synth has (8 voice polyphony).
    for (int i = 0; i < 8; ++i)
    {
        auto* voice = new FMVoice();   // 1. Instantiate the voice
        voice->initParameters (apvts); // 2. Initialize parameters while it's in scope
        synth.addVoice (voice);        // 3. Hand ownership over to the JUCE synth
    }

    synth.addSound (new FMSound());
}

FMPluginAudioProcessor::~FMPluginAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout FMPluginAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    juce::StringArray waveChoices { "Sine", "Triangle", "Saw", "Square", "Additive", "Filter"};
    juce::StringArray filterTypeChoices { "Lowpass", "Highpass", "Bandpass", "Comb" };
    
    // Generate parameters for each operator dynamically
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        // ALL KNOB SETTINGS ARE HERE
        juce::String opNum = juce::String (i + 1);
        // Wave
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "WAVE_" + opNum, 1 }, "Op " + opNum + " Waveform", waveChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "FILTER_TYPE_" + opNum, 1 }, "Op " + opNum + " Filter Type", filterTypeChoices, 0));
        // Tempo Sync
        params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "TEMPO_SYNC_" + opNum, 1 }, "Op " + opNum + " Tempo Sync", false));
        // Tuning Ratios, Phase, & Detune
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"RATIO_" + opNum, 1}, "Op " + opNum + " Ratio", 0.25f, 16.0f, 1.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"DETUNE_" + opNum, 1}, "Op " + opNum + " Detune", -50.0f, 50.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "PHASE_" + opNum, 1 }, "Op " + opNum + " Phase", 0.0f, 360.0f, 0.0f));
        // Filter (only Q)
        auto qRange = juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.5f);
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "FILTER_Q_" + opNum, 1 }, "Op " + opNum + " Filter Q", qRange, 0.707f));
        // Envelopes
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"ATTACK_" + opNum, 1}, "Op " + opNum + " Attack", 0.001f, 5.0f, 0.1f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"DECAY_" + opNum, 1}, "Op " + opNum + " Decay", 0.01f, 5.0f, 0.2f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"SUSTAIN_" + opNum, 1}, "Op " + opNum + " Sustain", 0.0f, 1.0f, 0.8f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"RELEASE_" + opNum, 1}, "Op " + opNum + " Release", 0.01f, 5.0f, 0.5f));
        // Output Levels
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"OUT_" + opNum, 1}, "Op " + opNum + " Out Level", 0.0f, 1.0f, (i == 0) ? 1.0f : 0.0f));
    }
    
    // Generate Modulation Matrix Nodes (NxN Grid)
    for (int src = 0; src < ProjectConfig::numOperators; ++src)
    {
        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            juce::String paramID = "MOD_" + juce::String (src) + "_" + juce::String (dest);
            juce::String name = "Mod Op " + juce::String (src + 1) + " -> Op " + juce::String (dest + 1);
            params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {paramID, 1}, name, 0.0f, 10.0f, 0.0f));
        }
    }
    
    // Audio Routing Matrix Nodes (NxN Grid)
    for (int src = 0; src < ProjectConfig::numOperators; ++src)
    {
        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            juce::String paramID = "AUDIO_ROUTE_" + juce::String (src) + "_" + juce::String (dest);
            juce::String name = "Audio Matrix: Op " + juce::String (src + 1) + " -> Op " + juce::String (dest + 1);
            params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {paramID, 1}, name, 0.0f, 1.0f, 0.0f));
        }
    }

    // --- NEW: FOCUSED MODULATION MATRIX PARAMETERS ---
    juce::StringArray targetChoices { "Pitch", "Phase", "Level", "Cutoff", "Resonance" };
    
    // Add target trackers for the 3 individual selection rows
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "ROW_TARGET_1", 1 }, "Row 1 Target", targetChoices, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "ROW_TARGET_2", 1 }, "Row 2 Target", targetChoices, 1));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "ROW_TARGET_3", 1 }, "Row 3 Target", targetChoices, 2));

    // Generate background multi-dimensional parameter nodes for row controls
    for (int row = 1; row <= 3; ++row)
    {
        for (int src = 0; src < ProjectConfig::numOperators; ++src)
        {
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
            {
                juce::String paramID = "FOCUSED_MOD_R" + juce::String (row) + "_" + juce::String (src) + "_" + juce::String (dest);
                juce::String name = "Row " + juce::String (row) + ": Op " + juce::String (src + 1) + " -> Op " + juce::String (dest + 1);
                
                // Allow positive/negative bi-directional modulation depths
                params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { paramID, 1 }, name, -1.0f, 1.0f, 0.0f));
            }
        }
    }
    // --------------------------------------------------

    // --- CHORUS PARAMETERS ---
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("CHORUS_MIX", "Chorus Mix", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("CHORUS_RATE", "Chorus Rate (Hz)", 0.1f, 5.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("CHORUS_DEPTH", "Chorus Depth", 0.0f, 1.0f, 0.25f));
    
    // --- DELAY PARAMETERS ---
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("DELAY_MIX", "Delay Mix", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("DELAY_TIME", "Delay Time (ms)", 50.0f, 1000.0f, 300.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("DELAY_FEEDBACK", "Delay Feedback", 0.0f, 0.95f, 0.3f));
    
    // --- REVERB PARAMETERS ---
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("REVERB_MIX", "Reverb Mix", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("REVERB_ROOM", "Room Size", 0.0f, 1.0f, 0.5f));
    
    // Gain
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
    juce::ParameterID { "GAIN_CEIL", 1 }, "Gain Ceiling", juce::NormalisableRange<float> (-24.0f, 0.0f, 0.1f),-0.2f)); // Ranges from -24dB to 0dB, default at -0.2dB
    
    // This line has to be the end of this function, otherwise stuff won't process
    return { params.begin(), params.end() };
}

void FMPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();
    // prepare synth voices
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
            voice->setCurrentPlaybackSampleRate (sampleRate); // Make sure your FMVoice class handles this parameter update!
    }
    // prepare effects with default values?
    chorusModule.prepare (spec);
    chorusModule.setCentreDelay (7.0f); // ms
    chorusModule.setFeedback (0.2f);
    chorusModule.setMix (0.0f);        // Controlled by APVTS later
    reverbModule.prepare (spec);
    reverbParams.roomSize = 0.5f;
    reverbParams.damping = 0.3f;
    reverbParams.wetLevel = 0.0f;       // Controlled by APVTS later
    reverbParams.dryLevel = 1.0f;
    reverbModule.setParameters (reverbParams);
    // Prepare Delay Buffers (Allocate enough memory for a 2-second maximum delay)
    delayBuffers.assign (spec.numChannels, std::vector<float>(static_cast<int>(sampleRate * 2.0f), 0.0f));
    delayWriteIndex = 0;
}

void FMPluginAudioProcessor::releaseResources() {}

void FMPluginAudioProcessor::updateVoices()
{
    // Read current atomic parameters from APVTS and safely pass them to our synthesis engine
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
        {
            // In a production build, add a thread-safe method inside `FMVoice` 
            // (e.g., voice->updateParameters(...)) to read these parameters atomically.
        }
    }
}

void FMPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Start process block
    juce::ScopedNoDenormals noDenormals;

    // tempo sync
    float activeBPM = 120.0f; // Default fallback
    if (auto* playHead = getPlayHead())
    {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
        {
            activeBPM = static_cast<float> (positionInfo->getBpm().orFallback (120.0));
        }
    }

    // 2. Safely push the BPM down into every active voice engine
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
        {
            voice->setDAWTempo (activeBPM);
        }
    }

    // Continue rendering
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    updateVoices();
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
    // effects
    // Create the DSP context wrapping your audio buffer
    juce::dsp::AudioBlock<float> block (buffer);
    // apply gain control
    if (auto* gainParam = apvts.getRawParameterValue ("GAIN_CEIL")) {
        float masterGainLinear = juce::Decibels::decibelsToGain (gainParam->load()); // e.g., -6.0 dB
        block.multiplyBy (masterGainLinear);
    }
    juce::dsp::ProcessContextReplacing<float> context (block);
    // chorus first
    if (auto* cMix = apvts.getRawParameterValue ("CHORUS_MIX"))
    if (auto* cRate = apvts.getRawParameterValue ("CHORUS_RATE"))
    if (auto* cDepth = apvts.getRawParameterValue ("CHORUS_DEPTH"))
    {
        chorusModule.setMix (cMix->load());
        chorusModule.setRate (cRate->load());
        chorusModule.setDepth (cDepth->load());
    }
    chorusModule.process (context);
    // delay second
    auto* dMixParam = apvts.getRawParameterValue ("DELAY_MIX");
    auto* dTimeParam = apvts.getRawParameterValue ("DELAY_TIME");
    auto* dFeedbackParam = apvts.getRawParameterValue ("DELAY_FEEDBACK");

    // CRITICAL SAFETY: Only proceed if parameters exist AND buffers are allocated
    if (dMixParam && dTimeParam && dFeedbackParam && !delayBuffers.empty() && delayBuffers[0].size() > 0)
    {
        float dMix = dMixParam->load();
        float dTimeMs = dTimeParam->load();
        float dFeedback = dFeedbackParam->load();

        // Only run the sample loop if the effect is actually turned up
        if (dMix > 0.001f)
        {
            int delaySampleLength = static_cast<int>((getSampleRate() * dTimeMs) / 1000.0f);

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                if (channel >= static_cast<int>(delayBuffers.size()))
                    continue;

                auto* channelData = buffer.getWritePointer (channel);
                auto& delayBuf = delayBuffers[channel];

                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    float inputSample = channelData[sample];
                    
                    int readIndex = (delayWriteIndex + sample - delaySampleLength + delayBuf.size()) % delayBuf.size();
                    float delayedSample = delayBuf[readIndex];

                    channelData[sample] = inputSample + (delayedSample * dMix);
                    delayBuf[(delayWriteIndex + sample) % delayBuf.size()] = inputSample + (delayedSample * dFeedback);
                }
            }
        }

        // Move the pointer safely inside the protection block
        delayWriteIndex = (delayWriteIndex + buffer.getNumSamples()) % delayBuffers[0].size();
    }
    // and reverb
    if (auto* rMix = apvts.getRawParameterValue ("REVERB_MIX"))
    if (auto* rRoom = apvts.getRawParameterValue ("REVERB_ROOM"))
    {
        float mixVal = rMix->load();
        reverbParams.wetLevel = mixVal;
        reverbParams.dryLevel = 1.0f - (mixVal * 0.5f); // Keep dry level full or slightly dipped
        reverbParams.roomSize = rRoom->load();
        reverbModule.setParameters (reverbParams);
    }
    reverbModule.setParameters (reverbParams);
    reverbModule.process (context);
    // emergency soft clipper. i tried a brickwall limiter and hoo boy does it not work
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] = std::tanh (channelData[sample]);
        }
    }
}

juce::AudioProcessorEditor* FMPluginAudioProcessor::createEditor()
{
    return new FMPluginAudioProcessorEditor (*this);
}

void FMPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void FMPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

// This function fixes your 'undefined reference to createPluginFilter()' error!
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FMPluginAudioProcessor();
}
