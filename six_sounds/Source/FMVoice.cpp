#include "FMVoice.h"

FMVoice::FMVoice()
{
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        for (int j = 0; j < ProjectConfig::numOperators; ++j)
        {
            matrixParams[i][j] = nullptr;
            audioMatrixParams[i][j] = nullptr;
            for (int row = 0; row < 3; ++row)
            {
                customModMatrixParams[row][i][j] = nullptr;
            }
        }
    }
}

bool FMVoice::canPlaySound (juce::SynthesiserSound* sound) 
{ 
    return true; 
}

void FMVoice::initParameters (juce::AudioProcessorValueTreeState& apvts)
{
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        juce::String opNum = juce::String (i + 1);

        auto check = [&](juce::String id, std::atomic<float>*& ptr) {
            ptr = apvts.getRawParameterValue(id);
            if (ptr == nullptr) 
                DBG("CRITICAL: Parameter " + id + " not found!");
        };

        check("MODE_" + opNum, opParams[i].mode);
        check("WAVE_SHAPE_" + opNum, opParams[i].wave);
        check("FILTER_TYPE_" + opNum, opParams[i].filterType);
        check("RATIO_" + opNum, opParams[i].ratio);
        check("DETUNE_" + opNum, opParams[i].detune);
        check("PHASE_" + opNum, opParams[i].phase);
        check("FOLD_" + opNum, opParams[i].fold);
        check("OUT_" + opNum, opParams[i].out);
        check("ATTACK_" + opNum, opParams[i].attack);
        check("DECAY_" + opNum, opParams[i].decay);
        check("SUSTAIN_" + opNum, opParams[i].sustain);
        check("RELEASE_" + opNum, opParams[i].release);
        check("TEMPO_SYNC_" + opNum, opParams[i].sync);
        
        check("MOD_SRC_" + opNum, modSrcParams[i]);
        check("MOD_TGT_" + opNum, modTgtParams[i]);
        check("MOD_AMT_" + opNum, modAmtParams[i]);

        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            matrixParams[i][dest] = apvts.getRawParameterValue ("MOD_" + juce::String (i) + "_" + juce::String (dest));
            audioMatrixParams[i][dest] = apvts.getRawParameterValue ("AUDIO_ROUTE_" + juce::String (i) + "_" + juce::String (dest));

            for (int row = 0; row < 3; ++row)
            {
                juce::String paramID = "FOCUSED_MOD_R" + juce::String(row + 1) + "_" + juce::String(i) + "_" + juce::String(dest);
                customModMatrixParams[row][i][dest] = apvts.getRawParameterValue (paramID);
            }
        }
    }
}

void FMVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    double safeRate = newRate > 0.0 ? newRate : 44100.0;
    
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        operators[i].prepare (safeRate);
    }
}

void FMVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    baseFrequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    level = velocity;
    
    resetVoiceState();

    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        if (opParams[i].attack != nullptr)
        {
            float initPhase = opParams[i].phase->load (std::memory_order_relaxed);
            operators[i].resetPhase (initPhase);
            
            juce::ADSR::Parameters p;
            p.attack  = opParams[i].attack->load (std::memory_order_relaxed);
            p.decay   = opParams[i].decay->load (std::memory_order_relaxed);
            p.sustain = opParams[i].sustain->load (std::memory_order_relaxed);
            p.release = opParams[i].release->load (std::memory_order_relaxed);
            
            operators[i].noteOn (p);
        }
    }
}

void FMVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff) 
    { 
        for (auto& op : operators) op.noteOff(); 
    }
    else 
    { 
        clearCurrentNote(); 
        resetVoiceState();
    }
}

void FMVoice::pitchWheelMoved (int) {}
void FMVoice::controllerMoved (int, int) {}

void FMVoice::setDAWTempo (float newBPM) noexcept
{
    currentBPM.store (newBPM, std::memory_order_relaxed);
}

void FMVoice::resetVoiceState()
{
    lastOpOutputs.fill (0.0f);
    previousOpOutputs.fill (0.0f);
    processedOpOutputs.fill (0.0f);

    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        operators[i].resetPhase (0.0f); 
    }
}

void FMVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!isVoiceActive()) return;

    float currentBpmValue = currentBPM.load(std::memory_order_relaxed);

    auto safeLoad = [](std::atomic<float>* ptr, float fallback = 0.0f) -> float {
        return ptr != nullptr ? ptr->load(std::memory_order_relaxed) : fallback;
    };

    // =====================================================================
    // 1. CACHE PARAMETERS
    // =====================================================================
    std::array<float, ProjectConfig::numOperators> cachedRatios, cachedDetunes, cachedPhases, cachedFolds, cachedOuts;
    std::array<int, ProjectConfig::numOperators> cachedModes, cachedShapes, cachedFilterTypes;
    std::array<bool, ProjectConfig::numOperators> cachedSyncs;
    std::array<float, ProjectConfig::numOperators> cachedAttacks, cachedDecays, cachedSustains, cachedReleases;

    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        cachedRatios[i]    = safeLoad(opParams[i].ratio, 1.0f); 
        cachedDetunes[i]   = safeLoad(opParams[i].detune, 0.0f);
        cachedPhases[i]    = safeLoad(opParams[i].phase, 0.0f);
        cachedFolds[i]     = safeLoad(opParams[i].fold, 0.0f);
        cachedOuts[i]      = safeLoad(opParams[i].out, 0.0f);
        cachedModes[i]       = static_cast<int>(safeLoad(opParams[i].mode, 0.0f));
        cachedShapes[i]      = static_cast<int>(safeLoad(opParams[i].wave, 0.0f));
        cachedFilterTypes[i] = static_cast<int>(safeLoad(opParams[i].filterType, 0.0f));
        cachedSyncs[i]     = safeLoad(opParams[i].sync, 0.0f) > 0.5f;
    }

    // =====================================================================
    // 2. SAMPLE-RATE DSP LOOP
    // =====================================================================
    for (int sample = 0; sample < numSamples; ++sample)
    {
        std::array<float, ProjectConfig::numOperators> opOutputs { 0.0f };
        std::array<float, ProjectConfig::numOperators> pitchModOffsets { 0.0f };
        std::array<float, ProjectConfig::numOperators> phaseModOffsets { 0.0f };
        std::array<float, ProjectConfig::numOperators> foldModOffsets { 0.0f };
        std::array<float, ProjectConfig::numOperators> levelModOffsets { 0.0f };
        std::array<float, ProjectConfig::numOperators> cutoffModOffsets { 0.0f };

        // --- PHASE A: MODULATION MATRIX PROCESSING ---
        for (int row = 0; row < 3; ++row)
        {
            int targetType = static_cast<int>(safeLoad(modTgtParams[row])); 

            for (int src = 0; src < ProjectConfig::numOperators; ++src)
            {
                float srcAudio = lastOpOutputs[src];
                for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
                {
                    float amt = safeLoad(customModMatrixParams[row][src][dest]); 
                    if (std::abs(amt) < 0.0001f) continue;

                    float modSignal = srcAudio * amt;
                    switch (targetType)
                    {
                        case 0: pitchModOffsets[dest]  += modSignal; break;
                        case 1: phaseModOffsets[dest]  += modSignal; break;
                        case 2: levelModOffsets[dest]  += modSignal; break;
                        case 3: cutoffModOffsets[dest] += modSignal; break;
                        case 4: foldModOffsets[dest]   += modSignal; break;
                    }
                }
            }
        }

        // --- PHASE B: OPERATOR PROCESSING ---
        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            float modulationSum = 0.0f;
            float audioInputSum = 0.0f;

            for (int src = 0; src < ProjectConfig::numOperators; ++src)
            {
                float modIndex = safeLoad(matrixParams[src][dest]); 
                if (modIndex > 0.0f)
                {
                    float modSignal = (src == dest) ? (lastOpOutputs[src] + previousOpOutputs[src]) * 0.5f : lastOpOutputs[src];
                    modulationSum += modSignal * modIndex;
                }

                float audioGain = safeLoad(audioMatrixParams[src][dest]); 
                if (audioGain > 0.0f)
                {
                    audioInputSum += lastOpOutputs[src] * audioGain;
                }
            }
            audioInputSum = std::tanh (audioInputSum);

            // Let the operator do ALL the heavy lifting!
            opOutputs[dest] = operators[dest].processSample (
                baseFrequency, 
                currentBpmValue,
                cachedRatios[dest], 
                cachedDetunes[dest], 
                cachedPhases[dest], 
                cachedFolds[dest],
                audioInputSum, 
                modulationSum,
                pitchModOffsets[dest], 
                phaseModOffsets[dest], 
                cutoffModOffsets[dest], 
                foldModOffsets[dest],
                cachedModes[dest], 
                cachedShapes[dest], 
                cachedFilterTypes[dest], 
                cachedSyncs[dest]
            );

            // --- PHASE C: FINAL LEVEL & HISTORY TRACKING ---
            processedOpOutputs[dest] = opOutputs[dest];

            if (std::abs(levelModOffsets[dest]) > 0.001f)
            {
                processedOpOutputs[dest] *= juce::jlimit(0.0f, 2.0f, 1.0f + levelModOffsets[dest]);
            }
        }

        previousOpOutputs = lastOpOutputs;
        lastOpOutputs = processedOpOutputs;

        // =====================================================================
        // 3. MAIN OUTPUT MIX BUS
        // =====================================================================
        float mixSample = 0.0f;
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            mixSample += processedOpOutputs[i] * cachedOuts[i];
        }

        float polyphonyCushion = 1.0f / std::sqrt (static_cast<float> (ProjectConfig::numOperators));
        float finalGain = level * polyphonyCushion * 0.5f;
        
        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
        {
            outputBuffer.addSample (channel, startSample + sample, mixSample * finalGain);
        }
    }

    // =====================================================================
    // 4. CLEANUP: CHECK IF VOICE FINISHED
    // =====================================================================
    bool isAnyEnvelopeActive = false;
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        if (operators[i].isActive())
        {
            isAnyEnvelopeActive = true;
            break;
        }
    }

    if (!isAnyEnvelopeActive)
    {
        clearCurrentNote();
        resetVoiceState();
    }
}
