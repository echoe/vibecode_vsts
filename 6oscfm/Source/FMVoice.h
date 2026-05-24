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
	// Filters
	f1ModeParam = apvts.getRawParameterValue ("FILTER1_MODE");
        f1FreqParam = apvts.getRawParameterValue ("FILTER1_FREQ");
        f1ResParam  = apvts.getRawParameterValue ("FILTER1_RES");
        
        f2ModeParam = apvts.getRawParameterValue ("FILTER2_MODE");
        f2FreqParam = apvts.getRawParameterValue ("FILTER2_FREQ");
        f2ResParam  = apvts.getRawParameterValue ("FILTER2_RES");

	//diag
	jassert (f1ModeParam != nullptr);
jassert (f1FreqParam != nullptr);
jassert (f1ResParam != nullptr);
jassert (f2ModeParam != nullptr);
jassert (f2FreqParam != nullptr);
jassert (f2ResParam != nullptr);
    }

    void setCurrentPlaybackSampleRate (double newRate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
        
        // Defensive check: Some hosts pass 0.0 initially during setup sequences
        double safeRate = newRate > 0.0 ? newRate : 44100.0;
        
        for (auto& op : operators)
            op.prepare (safeRate);
	filter1.prepare (newRate);
        filter2.prepare (newRate);
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
        }
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            for (auto& op : operators) 
                op.noteOff();
        }
        else
        {
            clearCurrentNote();
        }
    }

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        // Guard check: if parameters haven't mapped yet, exit gracefully without dropping the note
        if (ratioParams[0] == nullptr) return;

        // Main Sample-by-Sample DSP Loop
        for (int sample = 0; sample < numSamples; ++sample)
        {
            std::array<float, ProjectConfig::numOperators> opOutputs { 0.0f };

            // 1. Calculate FM cross-modulation grid
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
            {
                float modulationSum = 0.0f;
                for (int src = 0; src < ProjectConfig::numOperators; ++src)
                {
                    float modIndex = matrixParams[src][dest]->load (std::memory_order_relaxed);
                    modulationSum += lastOpOutputs[src] * modIndex;
                }

                float ratio = ratioParams[dest]->load (std::memory_order_relaxed);
                float detune = detuneParams[dest]->load (std::memory_order_relaxed);                int waveType = static_cast<int> (waveParams[dest]->load (std::memory_order_relaxed));

                opOutputs[dest] = operators[dest].processSample (baseFrequency, ratio, detune, modulationSum, waveType);
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

            // Add filters
	    bool filtersAreLinked = (f1ModeParam != nullptr && f1FreqParam != nullptr && f1ResParam != nullptr &&
                                     f2ModeParam != nullptr && f2FreqParam != nullptr && f2ResParam != nullptr);

	    // 2. Only process if the pointers actually exist
            if (filtersAreLinked)
            {
                int m1 = static_cast<int> (f1ModeParam->load (std::memory_order_relaxed));
                float fr1 = f1FreqParam->load (std::memory_order_relaxed);
                float r1 = f1ResParam->load (std::memory_order_relaxed);
            
                int m2 = static_cast<int> (f2ModeParam->load (std::memory_order_relaxed));
                float fr2 = f2FreqParam->load (std::memory_order_relaxed);
                float r2 = f2ResParam->load (std::memory_order_relaxed);
            
                mixSample = filter1.processSample (mixSample, m1, fr1, r1);
                mixSample = filter2.processSample (mixSample, m2, fr2, r2);
	    }

            // 3. Write data across available hardware output audio channels
            for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            {
                outputBuffer.addSample (channel, startSample + sample, mixSample * level * 0.4f);
            }

        }

        // Standard, robust lifecycle verification:
        // The voice stays alive as long as ANY operator's envelope is still processing.
        bool anyOscillatorActive = false;
        for (auto& op : operators)
        {
            if (op.isActive())
            {
                anyOscillatorActive = true;
                break;
            }
        }

        if (!anyOscillatorActive)
        {
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

private:
    double baseFrequency = 440.0;
    float level = 0.0f;

    std::array<FMOperator, ProjectConfig::numOperators> operators;
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

    SynthFilter filter1, filter2;

    std::atomic<float>* f1ModeParam = nullptr;
    std::atomic<float>* f1FreqParam = nullptr;
    std::atomic<float>* f1ResParam  = nullptr;
    
    std::atomic<float>* f2ModeParam = nullptr;
    std::atomic<float>* f2FreqParam = nullptr;
    std::atomic<float>* f2ResParam  = nullptr;
};
