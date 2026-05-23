#pragma once
#include <JuceHeader.h>
#include "FMOperator.h"

class FMVoice : public juce::SynthesiserVoice
{
public:
    static constexpr int numOperators = 2;

    FMVoice()
    {
        for (int i = 0; i < numOperators; ++i)
        {
	    waveParams[i] = nullptr;
            ratioParams[i] = nullptr;
            detuneParams[i] = nullptr;
            attackParams[i] = nullptr;
            decayParams[i] = nullptr;
            sustainParams[i] = nullptr;
            releaseParams[i] = nullptr;
            outParams[i] = nullptr;

            for (int j = 0; j < numOperators; ++j)
                matrixParams[i][j] = nullptr;
        }
    }

    bool canPlaySound (juce::SynthesiserSound* sound) override { return true; }

    void initParameters (juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < numOperators; ++i)
        {
            juce::String opNum = juce::String (i + 1);

	    waveParams[i] = apvts.getRawParameterValue ("WAVE_" + opNum);
            ratioParams[i]   = apvts.getRawParameterValue ("RATIO_" + opNum);
            detuneParams[i]  = apvts.getRawParameterValue ("DETUNE_" + opNum);
            attackParams[i]  = apvts.getRawParameterValue ("ATTACK_" + opNum);
            decayParams[i]   = apvts.getRawParameterValue ("DECAY_" + opNum);
            sustainParams[i] = apvts.getRawParameterValue ("SUSTAIN_" + opNum);
            releaseParams[i] = apvts.getRawParameterValue ("RELEASE_" + opNum);
            outParams[i]     = apvts.getRawParameterValue ("OUT_" + opNum);

            for (int dest = 0; dest < numOperators; ++dest)
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
        
        for (auto& op : operators)
            op.prepare (safeRate);
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        baseFrequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        level = velocity;

        for (int i = 0; i < numOperators; ++i)
        {
            if (attackParams[i] != nullptr)
            {
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
            std::array<float, numOperators> opOutputs { 0.0f };

            // 1. Calculate FM cross-modulation grid
            for (int dest = 0; dest < numOperators; ++dest)
            {
                float modulationSum = 0.0f;
                for (int src = 0; src < numOperators; ++src)
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
            for (int i = 0; i < numOperators; ++i)
            {
                float outLevel = outParams[i]->load (std::memory_order_relaxed);
                mixSample += opOutputs[i] * outLevel;
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

    std::array<FMOperator, numOperators> operators;
    std::array<float, numOperators> lastOpOutputs { 0.0f };

    std::array<std::atomic<float>*, numOperators> waveParams;
    std::array<std::atomic<float>*, numOperators> ratioParams;
    std::array<std::atomic<float>*, numOperators> detuneParams;
    std::array<std::atomic<float>*, numOperators> attackParams;
    std::array<std::atomic<float>*, numOperators> decayParams;
    std::array<std::atomic<float>*, numOperators> sustainParams;
    std::array<std::atomic<float>*, numOperators> releaseParams;
    std::array<std::atomic<float>*, numOperators> outParams;
    
    std::array<std::array<std::atomic<float>*, numOperators>, numOperators> matrixParams;
};
