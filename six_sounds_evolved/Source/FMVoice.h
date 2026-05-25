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
            attackParams[i] = nullptr;
            decayParams[i] = nullptr;
            sustainParams[i] = nullptr;
            releaseParams[i] = nullptr;
            outParams[i] = nullptr;

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
            attackParams[i]  = apvts.getRawParameterValue ("ATTACK_" + opNum);
            decayParams[i]   = apvts.getRawParameterValue ("DECAY_" + opNum);
            sustainParams[i] = apvts.getRawParameterValue ("SUSTAIN_" + opNum);
            releaseParams[i] = apvts.getRawParameterValue ("RELEASE_" + opNum);
            outParams[i]     = apvts.getRawParameterValue ("OUT_" + opNum);

            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
            {
                juce::String matrixID = "MOD_" + juce::String (i) + "_" + juce::String (dest);
                matrixParams[i][dest] = apvts.getRawParameterValue (matrixID);
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
                for (int src = 0; src < ProjectConfig::numOperators; ++src)
                {
                    float modIndex = matrixParams[src][dest]->load (std::memory_order_relaxed);
                    modulationSum += lastOpOutputs[src] * modIndex;
                }
    
                float ratio = ratioParams[dest]->load (std::memory_order_relaxed);
                int waveType = static_cast<int> (waveParams[dest]->load (std::memory_order_relaxed));
    
                if (waveType == 5) // If this operator is acting as a filter
                {
                    // AUDIO RATE MODULATION ENGINE:
                    // Use the modulation matrix sum to shift the filter cutoff directly!
                    float baseCutoff = baseFrequency * ratio;
                    
                    // Scale the modulation depth so it sounds musically useful (e.g., up to 5000Hz depth)
                    float cutoffModulation = modulationSum * 5000.0f; 
                    float dynamicCutoff = baseCutoff + cutoffModulation;
    
                    // Essential stability clamp so it never hits or exceeds Nyquist
                    float maxCutoff = static_cast<float>(safeSampleRate) * 0.49f;
                    dynamicCutoff = juce::jlimit (20.0f, maxCutoff, dynamicCutoff);
    
                    // For a filter node, what are we filtering? 
                    // Let's assume it filters the base voice frequency or external carrier input
                    float audioInput = baseFrequency * 0.1f; 
    
                    opOutputs[dest] = opFilters[dest].processSampleAudioRate (audioInput, dynamicCutoff, precalculatedK[dest]);
                }
                else
                {
                    float detune = detuneParams[dest]->load (std::memory_order_relaxed);
                    opOutputs[dest] = operators[dest].processSample (baseFrequency, ratio, detune, modulationSum, waveType);
                }
            }
            // Save state for single-sample feedback iterations
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

    std::array<FMOperator, ProjectConfig::numOperators> operators;
    std::atomic<float>* filterTypeParams[ProjectConfig::numOperators];
    SynthFilter opFilters[ProjectConfig::numOperators];
    std::array<float, ProjectConfig::numOperators> lastOpOutputs { 0.0f };

    std::array<std::atomic<float>*, ProjectConfig::numOperators> waveParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> ratioParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> detuneParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> phaseParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> attackParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> decayParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> sustainParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> releaseParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> outParams;
    
    std::array<std::array<std::atomic<float>*, ProjectConfig::numOperators>, ProjectConfig::numOperators> matrixParams;
    //from new code but maybe unnecessary
    float previousOpOutputs[ProjectConfig::numOperators] = { 0.0f };
    float currentNoteFrequency = 440.0f;
};
