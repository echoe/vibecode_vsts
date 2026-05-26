// FMVoice.h
#pragma once
#include <JuceHeader.h>
#include "FMOperator.h"
#include "SynthFilter.h"
#include "Constants.h"

class FMVoice : public juce::SynthesiserVoice
{
public:
    FMVoice()
    {
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
	    waveParams[i] = nullptr;
            ratioParams[i] = nullptr;
            detuneParams[i] = nullptr;
	    phaseParams[i] = nullptr;
	    outParams[i] = nullptr;
            attackParams[i] = nullptr;
            decayParams[i] = nullptr;
            sustainParams[i] = nullptr;
            releaseParams[i] = nullptr;
	    syncParams[i] = nullptr;

            for (int j = 0; j < ProjectConfig::numOperators; ++j)
                matrixParams[i][j] = nullptr;
        }
    }

    bool canPlaySound (juce::SynthesiserSound* sound) override { return true; }

    void initParameters (juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            juce::String opNum = juce::String (i + 1);

	    waveParams[i] = apvts.getRawParameterValue ("WAVE_" + opNum);
	    filterTypeParams[i] = apvts.getRawParameterValue ("FILTER_TYPE_" + opNum);
            ratioParams[i]   = apvts.getRawParameterValue ("RATIO_" + opNum);
            detuneParams[i]  = apvts.getRawParameterValue ("DETUNE_" + opNum);
	    phaseParams[i] = apvts.getRawParameterValue ("PHASE_" + opNum);
	    outParams[i]     = apvts.getRawParameterValue ("OUT_" + opNum);
            attackParams[i]  = apvts.getRawParameterValue ("ATTACK_" + opNum);
            decayParams[i]   = apvts.getRawParameterValue ("DECAY_" + opNum);
            sustainParams[i] = apvts.getRawParameterValue ("SUSTAIN_" + opNum);
            releaseParams[i] = apvts.getRawParameterValue ("RELEASE_" + opNum);
	    // filter
	    qParams[i] = apvts.getRawParameterValue ("FILTER_Q_" + opNum);
	    syncParams[i] = apvts.getRawParameterValue ("TEMPO_SYNC_" + opNum);

            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
            {
                juce::String matrixID = "MOD_" + juce::String (i) + "_" + juce::String (dest);
                matrixParams[i][dest] = apvts.getRawParameterValue (matrixID);
		juce::String audioMatrixID = "AUDIO_ROUTE_" + juce::String (i) + "_" + juce::String (dest);
                audioMatrixParams[i][dest] = apvts.getRawParameterValue (audioMatrixID);
            }
        }
    }

    void setCurrentPlaybackSampleRate (double newRate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
        
        // Defensive check: Some hosts pass 0.0 initially during setup sequences
        double safeRate = newRate > 0.0 ? newRate : 44100.0;
    
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            operators[i].prepare (safeRate); // ✓ Indexed correctly with a semicolon
            opFilters[i].prepare (safeRate); 
            opFilters[i].reset();
        }	
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        baseFrequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        level = velocity;
	lastOpOutputs.fill (0.0f);
	previousOpOutputs.fill (0.0f);

        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            if (attackParams[i] != nullptr)
            {
                float initPhase = phaseParams[i]->load (std::memory_order_relaxed);
                operators[i].resetPhase (initPhase);
		juce::ADSR::Parameters p;
                p.attack  = attackParams[i]->load (std::memory_order_relaxed);
                p.decay   = decayParams[i]->load (std::memory_order_relaxed);
                p.sustain = sustainParams[i]->load (std::memory_order_relaxed);
                p.release = releaseParams[i]->load (std::memory_order_relaxed);
                
                operators[i].noteOn (p);
            }
	    opFilters[i].reset(); // Keep your filters clean at note-on
        }
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) { for (auto& op : operators) op.noteOff(); }
        else { clearCurrentNote(); }
    }

    void pitchWheelMoved (int newPitchWheelValue) override {}
    void controllerMoved (int controllerNumber, int newControllerValue) override {}

    void setDAWTempo (float newBPM) noexcept
    {
        currentBPM.store (newBPM, std::memory_order_relaxed);
    }
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (ratioParams[0] == nullptr) return;
    
        double safeSampleRate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    
        // Precalculate resonance 'k' at block-rate to save precious CPU cycles
        std::array<float, ProjectConfig::numOperators> precalculatedK;
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            float detune = detuneParams[i]->load (std::memory_order_relaxed);
            float resonanceQ = 0.1f + (detune * 4.9f);
            precalculatedK[i] = 1.0f / resonanceQ;
            
            int filterTypeChoice = static_cast<int>(filterTypeParams[i]->load (std::memory_order_relaxed));
            opFilters[i].setType (filterTypeChoice);
        }
    
        // Main Sample Loop
        for (int sample = 0; sample < numSamples; ++sample)
        {
            std::array<float, ProjectConfig::numOperators> opOutputs { 0.0f };
        
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
            {
                float modulationSum = 0.0f;
                float audioInputSum = 0.0f;
        
                // Loop through all sources to read from both matrices simultaneously
                for (int src = 0; src < ProjectConfig::numOperators; ++src)
                {
                    // --- GRID 1: FM PHASE MODULATION MATRIX ---
                    float modIndex = matrixParams[src][dest]->load (std::memory_order_relaxed);
                    if (modIndex > 0.0f)
                    {
                        // Apply two-sample smoothing rule specifically for modulation feedback loops
                        float modSignal = (src == dest) ? (lastOpOutputs[src] + previousOpOutputs[src]) * 0.5f : lastOpOutputs[src];
                        modulationSum += modSignal * modIndex;
                    }
        
                    // --- GRID 2: RAW AUDIO ROUTING MATRIX ---
                    float audioGain = audioMatrixParams[src][dest]->load (std::memory_order_relaxed);
                    if (audioGain > 0.0f)
                    {
                        // Feed the raw un-smoothed audio output straight across the channel pipeline
                        audioInputSum += lastOpOutputs[src] * audioGain;
                    }
                }
        
                float ratio = ratioParams[dest]->load (std::memory_order_relaxed);
                int waveType = static_cast<int> (waveParams[dest]->load (std::memory_order_relaxed));
		if (waveType == 5) // If this operator is acting as a filter node
                {
                    // 1. Convert the linear 0.5 to 16.0 Ratio slider into a wide, logarithmic 20Hz to 20kHz absolute cutoff frequency
                    float ratioKnob = ratioParams[dest]->load (std::memory_order_relaxed);
                    float normalizedKnob = (ratioKnob - 0.25f) / (16.0f - 0.25f);
                    float baseCutoff = 20.0f * std::pow (1000.0f, normalizedKnob);
                    
                    // Saturate and scale the incoming matrix modulation sum
                    modulationSum = std::tanh (modulationSum * 0.2f) * 5.0f;
                    
                    // Handle Q / Resonance morphing. let's catch the error
		    float currentQ = 0.707f;
		    if (qParams[dest] != nullptr) { float currentQ = qParams[dest]->load (std::memory_order_relaxed); }
                    opFilters[dest].setResonance (currentQ);
                    float currentK = opFilters[dest].getPrecalculatedK();
                    
                    // Define the scaling factor for matrix modulation depth in Hz
                    float scaledDepthHz = 5000.0f; 
                
                    // Final dynamic cutoff calculation combining base frequency and matrix modulation
                    float dynamicCutoff = baseCutoff + (modulationSum * scaledDepthHz);
                
                    // Core safety clamp to prevent exceeding Nyquist limit
                    float maxCutoff = static_cast<float>(safeSampleRate) * 0.49f;
                    dynamicCutoff = juce::jlimit (20.0f, maxCutoff, dynamicCutoff);
                
                    // Filter the raw audio stream coming from the Routing Matrix!
                    opOutputs[dest] = opFilters[dest].processSampleAudioRate (audioInputSum, dynamicCutoff, currentK);
                }
		else // DESTINATION NODE IS A STANDARD OSCILLATOR
                {
                    // Apply traditional Phase Modulation
                    modulationSum = std::tanh (modulationSum * 0.2f) * 5.0f;
                    float detune = detuneParams[dest]->load (std::memory_order_relaxed);
		    float ratio = ratioParams[dest]->load (std::memory_order_relaxed);
		    float phaseOffset = phaseParams[dest]->load (std::memory_order_relaxed);
		    float nodeTargetFrequency = baseFrequency;
                    if (syncParams[dest] != nullptr && syncParams[dest]->load(std::memory_order_relaxed) > 0.5f)
                        {
                        float bpm = currentBPM.load (std::memory_order_relaxed);
                        nodeTargetFrequency = bpm / 60.0f; // Calculate 1/4 Note base tracking Hz
                    }
		    opOutputs[dest] = operators[dest].processSample (nodeTargetFrequency, ratio, detune, modulationSum, waveType);
                    opOutputs[dest] += audioInputSum;
                    
                }
            }

            // Update history buffers at the end of every sample iteration ---
            previousOpOutputs = lastOpOutputs;
            lastOpOutputs = opOutputs;
            // 2. Mix audible operators to output stream
            float mixSample = 0.0f;
            for (int i = 0; i < ProjectConfig::numOperators; ++i)
            {
                float outLevel = outParams[i]->load (std::memory_order_relaxed);
                mixSample += opOutputs[i] * outLevel;
            }
            // 3. Write data across available hardware output audio channels
            for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            {
                // Using your original clean write command:
                outputBuffer.addSample (channel, startSample + sample, mixSample * level * 0.4f);
            }
	}
        // The voice stays alive as long as ANY operator's envelope is still processing.
        bool anyOscillatorActive = false;
        for (auto& op : operators) { if (op.isActive()) { anyOscillatorActive = true; break; } }
        if (!anyOscillatorActive){ clearCurrentNote();}
    }

private:
    double baseFrequency = 440.0;
    float level = 0.0f;
    std::atomic<float> currentBPM { 120.0f }; // Safe default

    std::array<FMOperator, ProjectConfig::numOperators> operators;
    std::atomic<float>* filterTypeParams[ProjectConfig::numOperators];
    SynthFilter opFilters[ProjectConfig::numOperators];
    std::array<float, ProjectConfig::numOperators> lastOpOutputs { 0.0f };
    std::array<float, ProjectConfig::numOperators> previousOpOutputs { 0.0f };

    std::array<std::atomic<float>*, ProjectConfig::numOperators> waveParams;
    // OscParams
    std::array<std::atomic<float>*, ProjectConfig::numOperators> ratioParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> detuneParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> phaseParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> outParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> attackParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> decayParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> sustainParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> releaseParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> syncParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> qParams;
    
    std::array<std::array<std::atomic<float>*, ProjectConfig::numOperators>, ProjectConfig::numOperators> matrixParams;
    std::array<std::array<std::atomic<float>*, ProjectConfig::numOperators>, ProjectConfig::numOperators> audioMatrixParams;
};
