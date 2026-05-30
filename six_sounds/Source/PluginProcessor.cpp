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
    // Cache effect param pointers — must happen after createParameterLayout() runs
    // (which it does via the apvts initialiser above)
    chorusMixParam     = apvts.getRawParameterValue ("CHORUS_MIX");
    chorusRateParam    = apvts.getRawParameterValue ("CHORUS_RATE");
    chorusDepthParam   = apvts.getRawParameterValue ("CHORUS_DEPTH");
    delayMixParam      = apvts.getRawParameterValue ("DELAY_MIX");
    delayTimeParam     = apvts.getRawParameterValue ("DELAY_TIME");
    delayFeedbackParam = apvts.getRawParameterValue ("DELAY_FEEDBACK");
    reverbMixParam     = apvts.getRawParameterValue ("REVERB_MIX");
    reverbRoomParam    = apvts.getRawParameterValue ("REVERB_ROOM");

    // Sanity check — these will print if a param ID is mistyped
    jassert (chorusMixParam     != nullptr);
    jassert (chorusRateParam    != nullptr);
    jassert (chorusDepthParam   != nullptr);
    jassert (delayMixParam      != nullptr);
    jassert (delayTimeParam     != nullptr);
    jassert (delayFeedbackParam != nullptr);
    jassert (reverbMixParam     != nullptr);
    jassert (reverbRoomParam    != nullptr);
}

FMPluginAudioProcessor::~FMPluginAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout FMPluginAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    juce::StringArray modeChoices { "Wave", "Additive", "Filter" };
    juce::StringArray waveShapeChoices { "Sine", "Triangle", "Saw", "Square", "White Noise" };
    juce::StringArray filterTypeChoices { "Lowpass", "Highpass", "Bandpass", "Comb", "Granular" };
    
    // Generate parameters for each operator dynamically
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        // ALL KNOB SETTINGS ARE HERE
        juce::String opNum = juce::String (i + 1);
        // Wave
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "MODE_" + opNum, 1 }, "Op " + opNum + " Mode", modeChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "WAVE_SHAPE_" + opNum, 1 }, "Op " + opNum + " Wave Shape", waveShapeChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "FILTER_TYPE_" + opNum, 1 }, "Op " + opNum + " Filter Type", filterTypeChoices, 0));
        // Tempo Sync
        params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "TEMPO_SYNC_" + opNum, 1 }, "Op " + opNum + " Tempo Sync", false));
        // Osc Settings - Ratios, Detune, Phase, Fold. We repurpose these for filter
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"RATIO_" + opNum, 1}, "Op " + opNum + " Ratio", 0.01f, 16.0f, 1.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"DETUNE_" + opNum, 1}, "Op " + opNum + " Detune", -50.0f, 50.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "PHASE_" + opNum, 1 }, "Op " + opNum + " Phase", 0.0f, 360.0f, 0.0f));
	params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "FOLD_" + opNum, 1 }, "Op " + opNum + " Phase", 0.0f, 1.0f, 0.0f));
        // Envelopes
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"ATTACK_" + opNum, 1}, "Op " + opNum + " Attack", 0.001f, 5.0f, 0.1f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"DECAY_" + opNum, 1}, "Op " + opNum + " Decay", 0.01f, 5.0f, 0.2f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"SUSTAIN_" + opNum, 1}, "Op " + opNum + " Sustain", 0.0f, 1.0f, 0.8f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"RELEASE_" + opNum, 1}, "Op " + opNum + " Release", 0.01f, 5.0f, 0.5f));
        // Output Levels
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID {"OUT_" + opNum, 1}, "Op " + opNum + " Out Level", 0.0f, 1.0f, (i == 0) ? 1.0f : 0.0f));

	//
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

    // --- MODULATION MATRIX: 6 SLOTS ---
    // Sources: None + 6 Operators = 7 choices (index 0 = None, 1-6 = Op 1-6)
    // Targets: None + 5 params x 6 ops + 8 effects = 39 choices
    juce::StringArray modSourceChoices {
        "None",
        "Op 1", "Op 2", "Op 3", "Op 4", "Op 5", "Op 6"
    };
    juce::StringArray modTargetChoices {
        "None",
        // Per-operator targets (5 params x 6 ops = 30)
        "Op 1 Ratio",  "Op 1 Detune", "Op 1 Phase", "Op 1 Fold", "Op 1 Level",
        "Op 2 Ratio",  "Op 2 Detune", "Op 2 Phase", "Op 2 Fold", "Op 2 Level",
        "Op 3 Ratio",  "Op 3 Detune", "Op 3 Phase", "Op 3 Fold", "Op 3 Level",
        "Op 4 Ratio",  "Op 4 Detune", "Op 4 Phase", "Op 4 Fold", "Op 4 Level",
        "Op 5 Ratio",  "Op 5 Detune", "Op 5 Phase", "Op 5 Fold", "Op 5 Level",
        "Op 6 Ratio",  "Op 6 Detune", "Op 6 Phase", "Op 6 Fold", "Op 6 Level",
        // Effects targets (8)
        "Chorus Mix", "Chorus Rate", "Chorus Depth",
        "Delay Mix",  "Delay Time",  "Delay Feedback",
        "Reverb Mix", "Reverb Room"
    };
    
    for (int slot = 1; slot <= 6; ++slot)
    {
        juce::String s = juce::String (slot);
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "MOD_SRC_" + s, 1 },
            "Mod Source " + s, modSourceChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "MOD_TGT_" + s, 1 },
            "Mod Target " + s, modTargetChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "MOD_AMT_" + s, 1 },
            "Mod Amount " + s, -1.0f, 1.0f, 0.0f));
    }

    // Generate background multi-dimensional parameter nodes for row controls and mod matrix parameters
    for (int row = 1; row <= 6; ++row)
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
    juce::dsp::ProcessSpec delaySpec;
    delaySpec.sampleRate       = sampleRate;
    delaySpec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    delaySpec.numChannels      = 1; // Each line handles one channel
    
    delayLineL.prepare (delaySpec);
    delayLineR.prepare (delaySpec);
    delayLineL.reset();
    delayLineR.reset();
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
    juce::ScopedNoDenormals noDenormals;

    // =====================================================================
    // 1. TEMPO SYNC
    // =====================================================================
    float activeBPM = 120.0f;
    if (auto* playHead = getPlayHead())
    {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
            activeBPM = static_cast<float> (positionInfo->getBpm().orFallback (120.0));
    }

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
            voice->setDAWTempo (activeBPM);

    // =====================================================================
    // 2. CLEAR UNUSED OUTPUT CHANNELS & RENDER SYNTH
    // =====================================================================
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    updateVoices();
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    // =====================================================================
    // 3. MODULATION MATRIX: READ BASE PARAMS + ACCUMULATE VOICE MOD OFFSETS
    // =====================================================================
    float totalChorusMix     = chorusMixParam->load     (std::memory_order_relaxed);
    float totalChorusRate    = chorusRateParam->load    (std::memory_order_relaxed);
    float totalChorusDepth   = chorusDepthParam->load   (std::memory_order_relaxed);
    float totalDelayMix      = delayMixParam->load      (std::memory_order_relaxed);
    float totalDelayTime     = delayTimeParam->load     (std::memory_order_relaxed);
    float totalDelayFeedback = delayFeedbackParam->load (std::memory_order_relaxed);
    float totalReverbMix     = reverbMixParam->load     (std::memory_order_relaxed);
    float totalReverbRoom    = reverbRoomParam->load    (std::memory_order_relaxed);

    for (int v = 0; v < synth.getNumVoices(); ++v)
    {
        if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (v)))
        {
            if (voice->isVoiceActive())
            {
                totalChorusMix     += voice->chorusMixMod.load     (std::memory_order_relaxed);
                totalChorusRate    += voice->chorusRateMod.load    (std::memory_order_relaxed);
                totalChorusDepth   += voice->chorusDepthMod.load   (std::memory_order_relaxed);
                totalDelayMix      += voice->delayMixMod.load      (std::memory_order_relaxed);
                totalDelayTime     += voice->delayTimeMod.load     (std::memory_order_relaxed);
                totalDelayFeedback += voice->delayFeedbackMod.load (std::memory_order_relaxed);
                totalReverbMix     += voice->reverbMixMod.load     (std::memory_order_relaxed);
                totalReverbRoom    += voice->reverbRoomMod.load    (std::memory_order_relaxed);
            }
        }
    }

    // Clamp to safe ranges matching parameter definitions
    totalChorusMix     = juce::jlimit (0.0f,   1.0f,    totalChorusMix);
    totalChorusRate    = juce::jlimit (0.1f,   5.0f,    totalChorusRate);
    totalChorusDepth   = juce::jlimit (0.0f,   1.0f,    totalChorusDepth);
    totalDelayMix      = juce::jlimit (0.0f,   1.0f,    totalDelayMix);
    totalDelayTime     = juce::jlimit (50.0f,  1000.0f, totalDelayTime);
    totalDelayFeedback = juce::jlimit (0.0f,   0.95f,   totalDelayFeedback);
    totalReverbMix     = juce::jlimit (0.0f,   1.0f,    totalReverbMix);
    totalReverbRoom    = juce::jlimit (0.0f,   1.0f,    totalReverbRoom);

    // =====================================================================
    // 4. MASTER GAIN
    // =====================================================================
    juce::dsp::AudioBlock<float> block (buffer);

    if (auto* gainParam = apvts.getRawParameterValue ("GAIN_CEIL"))
        block.multiplyBy (juce::Decibels::decibelsToGain (gainParam->load()));

    // =====================================================================
    // 5. CHORUS
    // =====================================================================
    chorusModule.setMix   (totalChorusMix);
    chorusModule.setRate  (totalChorusRate);
    chorusModule.setDepth (totalChorusDepth);

    juce::dsp::ProcessContextReplacing<float> context (block);
    chorusModule.process (context);

    // =====================================================================
    // 6. DELAY
    // =====================================================================
    {
        float delaySamples = (totalDelayTime / 1000.0f) * static_cast<float> (getSampleRate());
        delaySamples = juce::jlimit (1.0f,
                                     static_cast<float> (delayLineL.getMaximumDelayInSamples()),
                                     delaySamples);

        delayLineL.setDelay (delaySamples);
        delayLineR.setDelay (delaySamples);

        auto* leftData   = buffer.getWritePointer (0);
        auto* rightData  = buffer.getWritePointer (1);
        int   numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            float delayedL = delayLineL.popSample (0);
            float delayedR = delayLineR.popSample (0);

            delayLineL.pushSample (0, leftData[i]  + delayedL * totalDelayFeedback);
            delayLineR.pushSample (0, rightData[i] + delayedR * totalDelayFeedback);

            leftData[i]  += delayedL * totalDelayMix;
            rightData[i] += delayedR * totalDelayMix;
        }
    }

    // =====================================================================
    // 7. REVERB
    // =====================================================================
    reverbParams.wetLevel = totalReverbMix;
    reverbParams.dryLevel = 1.0f - (totalReverbMix * 0.5f);
    reverbParams.roomSize = totalReverbRoom;
    reverbModule.setParameters (reverbParams);
    reverbModule.process (context);

    // =====================================================================
    // 8. OUTPUT SOFT CLIP
    // =====================================================================
    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            channelData[sample] = std::tanh (channelData[sample] * 0.4f);
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
// avoiding issues when looping sounds in VSTs
void FMPluginAudioProcessor::reset()
{
    // 1. Clear the custom delay vectors completely
    delayLineL.reset();
    delayLineR.reset();

    // 2. Clear JUCE native effects states
    chorusModule.reset();
    reverbModule.reset();
    
    // 3. Tell the synth engine to kill hanging voice notes
    synth.allNotesOff (0, false);
    // also clear filters, which aren't cleared by the above
    for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* voice = dynamic_cast<FMVoice*> (synth.getVoice (i)))
            {
                voice->resetVoiceState();
            }
        }
}

// Necessary to avoid createPluginFilter()' error
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FMPluginAudioProcessor();
}
