// FMVoice.cpp
#include "FMVoice.h"

FMVoice::FMVoice()
{
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        // Pre-allocate base filters to avoid audio thread allocations later
	// The CombFilter inherest SynthFilter which means we can call SynthFilter through it. This feels backwards
        opFilters[i] = std::make_unique<SynthFilter>(); 
        
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

        // Helper lambda for safe parameter linking
        auto check = [&](juce::String id, std::atomic<float>*& ptr) {
            ptr = apvts.getRawParameterValue(id);
            if (ptr == nullptr) 
                DBG("CRITICAL: Parameter " + id + " not found!");
            else
                DBG("Parameter " + id + " found!");
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
        if (opFilters[i] != nullptr)
        {
            opFilters[i]->prepare (safeRate);
            opFilters[i]->reset();
        }
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
	if (opFilters[i] != nullptr) opFilters[i]->noteStarted();
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
        if (opFilters[i] != nullptr) opFilters[i]->reset(); 
        operators[i].resetPhase (0.0f); 
    }
}

void FMVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!isVoiceActive()) return;

    double safeSampleRate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    float currentBpmValue = currentBPM.load(std::memory_order_relaxed);
    // get currentNote
    // =====================================================================
    // BULLETPROOF PARAMETER LOADER
    // If a pointer is null, it returns a safe default instead of crashing
    // =====================================================================
    auto safeLoad = [](std::atomic<float>* ptr, float fallback = 0.0f) -> float {
        return ptr != nullptr ? ptr->load(std::memory_order_relaxed) : fallback;
    };

    // =====================================================================
    // 1. CACHE PARAMETERS (CPU Optimization)
    // =====================================================================
    std::array<float, ProjectConfig::numOperators> cachedRatios, cachedDetunes, cachedPhases, cachedFolds, cachedOuts;
    std::array<int, ProjectConfig::numOperators> cachedModes, cachedShapes, cachedFilterTypes;
    std::array<bool, ProjectConfig::numOperators> cachedSyncs;
    std::array<float, ProjectConfig::numOperators> cachedAttacks, cachedDecays, cachedSustains, cachedReleases;

    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        // Now safely loading everything!
        cachedRatios[i]    = safeLoad(opParams[i].ratio, 1.0f); // Default ratio to 1
        cachedDetunes[i]   = safeLoad(opParams[i].detune, 0.0f);
        cachedPhases[i]    = safeLoad(opParams[i].phase, 0.0f);
	DBG("Op " << i << " phase ptr: " << (opParams[i].phase != nullptr ? "valid" : "null") 
    << "  raw value: " << (opParams[i].phase != nullptr ? opParams[i].phase->load() : -999.0f)
    << "  cachedPhases: " << cachedPhases[i]);
	cachedFolds[i]     = safeLoad(opParams[i].fold, 0.0f);
        cachedOuts[i]      = safeLoad(opParams[i].out, 0.0f);
        cachedModes[i]       = static_cast<int>(safeLoad(opParams[i].mode, 0.0f));
        cachedShapes[i]      = static_cast<int>(safeLoad(opParams[i].wave, 0.0f));
        cachedFilterTypes[i] = static_cast<int>(safeLoad(opParams[i].filterType, 0.0f));
        cachedSyncs[i]     = safeLoad(opParams[i].sync, 0.0f) > 0.5f;
        cachedAttacks[i]   = safeLoad(opParams[i].attack, 0.1f);
        cachedDecays[i]    = safeLoad(opParams[i].decay, 0.1f);
        cachedSustains[i]  = safeLoad(opParams[i].sustain, 1.0f);
        cachedReleases[i]  = safeLoad(opParams[i].release, 0.1f);
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
        std::array<float, ProjectConfig::numOperators> qModOffsets { 0.0f };

        // --- PHASE A: MODULATION MATRIX PROCESSING ---
        for (int row = 0; row < 3; ++row)
        {
            int targetType = static_cast<int>(safeLoad(modTgtParams[row])); // <-- SAFE

            for (int src = 0; src < ProjectConfig::numOperators; ++src)
            {
                float srcAudio = lastOpOutputs[src];

                for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
                {
                    float amt = safeLoad(customModMatrixParams[row][src][dest]); // <-- SAFE
                    if (std::abs(amt) < 0.0001f) continue;

                    float modSignal = srcAudio * amt;

                    switch (targetType)
                    {
                        case 0: pitchModOffsets[dest]  += modSignal; break;
                        case 1: phaseModOffsets[dest]  += modSignal; break;
                        case 2: levelModOffsets[dest]  += modSignal; break;
                        case 3: cutoffModOffsets[dest] += modSignal; break;
                        case 4: qModOffsets[dest]      += modSignal; break;
                        case 5: foldModOffsets[dest]   += modSignal; break;
                    }
                }
            }
        }

	// --- PHASE B: OPERATOR PROCESSING ---
        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            float modulationSum = 0.0f;
            float audioInputSum = 0.0f;

            // Gather incoming signals for this specific operator
            for (int src = 0; src < ProjectConfig::numOperators; ++src)
            {
                float modIndex = safeLoad(matrixParams[src][dest]); // <-- SAFE
                if (modIndex > 0.0f)
                {
                    float modSignal = (src == dest) ? (lastOpOutputs[src] + previousOpOutputs[src]) * 0.5f : lastOpOutputs[src];
                    modulationSum += modSignal * modIndex;
                }

                float audioGain = safeLoad(audioMatrixParams[src][dest]); // <-- SAFE
                if (audioGain > 0.0f)
                {
                    audioInputSum += lastOpOutputs[src] * audioGain;
                }
            }
            audioInputSum = std::tanh (audioInputSum);
            // Fetch Cached Variables
            float ratio = cachedRatios[dest];
            int opMode = cachedModes[dest];
            int opShape = cachedShapes[dest];
            int currentFilterType = cachedFilterTypes[dest];

            // =========================================================
            // CORE DSP DISPATCH
            // =========================================================

            // MODE 2: FILTER
	    if (opMode == 2 && opFilters[dest] != nullptr)
            {
                // --- 1. PREPARE PARAMETERS ---
                float feedbackAmt = juce::jlimit(-0.95f, 0.95f, (cachedFolds[dest] * 2.0f) - 1.0f);
		float rawDampingKnob = cachedDetunes[dest];
		float shiftedDampingKnob = rawDampingKnob + 50.0f;
		float normalizedDamping = shiftedDampingKnob / 100.0f;
                float dampingAmt  = juce::jlimit(0.001f, 0.95f, normalizedDamping);
                float keytrackAmt = cachedPhases[dest] / 360.0f; // normalize to 0.0 to 1.0
            
                float normalizedKnob = (ratio - 0.25f) / (16.0f - 0.25f);
                float baseFreq       = 20.0f * std::pow (1000.0f, normalizedKnob);
            
                // --- 2. APPLY KEYTRACKING ---
                float tunedFreq = baseFreq + keytrackAmt * (baseFrequency - baseFreq);
		// --- 3. FILTER MODE PROCESSING ---
                if (currentFilterType == 3) // Comb Filter
                {
                    	// Add Modulation to the tuned frequency
		    	float modDepth = 1.0f - keytrackAmt;
    		    	float combFreq = tunedFreq + modDepth * (modulationSum * 200.0f + cutoffModOffsets[dest] * 4000.0f);
    		    	combFreq = juce::jlimit(20.0f, static_cast<float>(safeSampleRate) * 0.49f, combFreq);
    		    	DBG("  baseFreq: " << baseFreq
    			<< "  keytrackAmt: " << keytrackAmt
    			<< "  cachedPhases[dest]: " << cachedPhases[dest]
    			<< "  combFreq: " << combFreq);

			float output = opFilters[dest]->processSampleComb(audioInputSum, combFreq, feedbackAmt, dampingAmt);
    			opOutputs[dest] = std::isfinite(output) ? std::tanh(output) : 0.0f;
                }
                else // SVF Filter
                {
                    modulationSum = std::tanh (modulationSum * 0.2f) * 5.0f;
            
                    // Coupled Resonance (Non-linear curve)
                    float coupledResonance = dampingAmt * dampingAmt;
                    opFilters[dest]->setResonance (coupledResonance);
            
                    float currentK = opFilters[dest]->getPrecalculatedK();
            
                    // Add Modulation to the tuned frequency
                    float dynamicCutoff = tunedFreq + (modulationSum * 5000.0f) + (cutoffModOffsets[dest] * 4000.0f);
                    dynamicCutoff = juce::jlimit (20.0f, static_cast<float>(safeSampleRate) * 0.49f, dynamicCutoff);
            
                    opOutputs[dest] = opFilters[dest]->processSampleAudioRate (audioInputSum, dynamicCutoff, currentK);
                }
            }
            // MODES 0 & 1: WAVE / ADDITIVE
            else
            {
                float nodeTargetFrequency = baseFrequency;
                if (cachedSyncs[dest])
                {
                    nodeTargetFrequency = currentBpmValue / 60.0f;
                }

                float semitoneOffset = pitchModOffsets[dest] * 12.0f;
                float modulatedFreq = nodeTargetFrequency * std::pow (2.0f, semitoneOffset / 12.0f);
                modulatedFreq = juce::jlimit(0.1f, static_cast<float>(safeSampleRate) * 0.49f, modulatedFreq);

                modulationSum = std::tanh (modulationSum * 0.15f) * (juce::MathConstants<float>::pi * 2.0f);

                float phaseOffset = cachedPhases[dest] + (phaseModOffsets[dest] * 360.0f);
                float detune = cachedDetunes[dest];

                // Generate Raw Oscillator Sample
                // IMPORTANT: Ensure operators[dest].processSample() internally calls adsr.getNextSample()!
                float rawSample = operators[dest].processSample (
                    modulatedFreq, ratio, detune, modulationSum,
                    audioInputSum, opMode, opShape, currentFilterType, phaseOffset
                );
		if (opMode == 0) // WAVE
                {
                    float finalFoldDepth = juce::jlimit (0.0f, 1.0f, cachedFolds[dest] + foldModOffsets[dest]);
                
                    // Only apply the Sine-folder if the user is actually turning the Fold knob
                    if (finalFoldDepth > 0.001f)
                    {
                        float drive = 1.0f + (finalFoldDepth * 5.0f);
                        float foldedSample = std::sin (rawSample * drive) * (1.0f / std::sqrt (drive));
                        opOutputs[dest] = std::tanh (foldedSample + audioInputSum);
                    }
                    else
                    {
                        // Pass the pure Saw/Triangle/Square shape through!
                        opOutputs[dest] = std::tanh (rawSample + audioInputSum);
                    }
                }
                else if (opMode == 1) // ADDITIVE
                {
                    opOutputs[dest] = std::tanh (rawSample + audioInputSum);
                }
            }

            // --- PHASE C: FINAL LEVEL & HISTORY TRACKING ---
            processedOpOutputs[dest] = opOutputs[dest];

            if (std::abs(levelModOffsets[dest]) > 0.001f)
            {
                processedOpOutputs[dest] *= juce::jlimit(0.0f, 2.0f, 1.0f + levelModOffsets[dest]);
            }
        }

        // Update history for the next frame
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
    // Only check envelope activity AFTER the block is completely processed
    bool isAnyEnvelopeActive = false;
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        if (operators[i].isActive())
        {
            isAnyEnvelopeActive = true;
            break;
        }
    }

    // If all ADSRs are completely finished, free the voice
    if (!isAnyEnvelopeActive)
    {
        clearCurrentNote();
        resetVoiceState();
    }
}
