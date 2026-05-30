#pragma once
#include <JuceHeader.h>
#include "SynthFilter.h"

struct FMOperator
{
    void prepare (double sampleRate)
    {
        currentSampleRate = sampleRate;
        envelope.setSampleRate (sampleRate);
        phase = 0.0;        
        
        // Prepare your custom unified filter
        internalFilter.prepare (sampleRate);
        internalFilter.reset();
    }
    
    void noteOn (const juce::ADSR::Parameters& envParams)
    {
        envelope.setParameters (envParams);
        envelope.noteOn();
        phase = 0.0;
        
        internalFilter.reset();
    }
    
    void noteOff() { envelope.noteOff(); }
    
    void resetPhase (float phaseInDegrees) 
    { 
        phase = (phaseInDegrees / 360.0) * juce::MathConstants<double>::twoPi; 
    }
    
    void resetVoiceState() 
    {
        envelope.reset();
        internalFilter.reset(); 
    }

    // Clean, comprehensive DSP processing!
    float processSample (double baseFrequency, float currentBpm,
                         float ratio, float detune, float phaseKnob, float foldKnob,
                         float audioInputSum, float modulationSum,
                         float pitchModOffset, float phaseModOffset, float cutoffModOffset, float foldModOffset,
                         int mode, int waveShape, int filterType, bool isSynced)
    {
        if (!envelope.isActive()) return 0.0f;
        
        float outputSample = 0.0f;
        
        // =========================================================
        // MODE 2: FILTER
        // =========================================================
        if (mode == 2)
        {
            // --- 1. PREPARE PARAMETERS ---
            float feedbackAmt = juce::jlimit(-0.95f, 0.95f, (foldKnob * 2.0f) - 1.0f);
            float shiftedDampingKnob = detune + 50.0f;
            float normalizedDamping = shiftedDampingKnob / 100.0f;
            float dampingAmt  = juce::jlimit(0.001f, 0.95f, normalizedDamping);
            float keytrackAmt = phaseKnob / 360.0f; 
        
            float normalizedKnob = (ratio - 0.25f) / (16.0f - 0.25f);
            float baseFreq       = 20.0f * std::pow (1000.0f, normalizedKnob);
            
            // --- 2. APPLY KEYTRACKING ---
            float tunedFreq = baseFreq + keytrackAmt * (baseFrequency - baseFreq);

            // --- 3. FILTER DSP ---
            if (filterType == 3) // Comb Filter
            {
                float modDepth = 1.0f - keytrackAmt;
                float combFreq = tunedFreq + modDepth * (modulationSum * 200.0f + cutoffModOffset * 4000.0f);
                combFreq = juce::jlimit(20.0f, static_cast<float>(currentSampleRate) * 0.49f, combFreq);

                float output = internalFilter.processSampleComb(audioInputSum, combFreq, feedbackAmt, dampingAmt);
                outputSample = std::isfinite(output) ? std::tanh(output) : 0.0f;
            }
            else // SVF Filter
            {
                float processedModSum = std::tanh (modulationSum * 0.2f) * 5.0f;
                
                // Coupled Resonance (Non-linear curve)
                float coupledResonance = dampingAmt * dampingAmt;
                internalFilter.setResonance (coupledResonance);
        
                float currentK = internalFilter.getPrecalculatedK();
        
                float dynamicCutoff = tunedFreq + (processedModSum * 5000.0f) + (cutoffModOffset * 4000.0f);
                dynamicCutoff = juce::jlimit (20.0f, static_cast<float>(currentSampleRate) * 0.49f, dynamicCutoff);
        
                outputSample = internalFilter.processSampleAudioRate (audioInputSum, dynamicCutoff, currentK);
            }
        }
        // =========================================================
        // MODES 0 & 1: WAVE / ADDITIVE
        // =========================================================
        else
        {
            // Sync & Pitch Processing
            float nodeTargetFrequency = baseFrequency;
            if (isSynced)
            {
                nodeTargetFrequency = currentBpm / 60.0f;
            }

            float semitoneOffset = pitchModOffset * 12.0f;
            float modulatedFreq = nodeTargetFrequency * std::pow (2.0f, semitoneOffset / 12.0f);
            modulatedFreq = juce::jlimit(0.1f, static_cast<float>(currentSampleRate) * 0.49f, modulatedFreq);

            // Phase Increment
            double phaseIncrement = (modulatedFreq * juce::MathConstants<double>::twoPi) / currentSampleRate;
            phase += phaseIncrement;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;

            // Phase Modulation & Offset
            float processedModSum = std::tanh (modulationSum * 0.15f) * (juce::MathConstants<float>::pi * 2.0f);
            float phaseOffset = phaseKnob + (phaseModOffset * 360.0f);
            float phaseOffsetRad = (phaseOffset / 360.0f) * juce::MathConstants<float>::twoPi;

            float lookupPhase = static_cast<float> (phase) + processedModSum + phaseOffsetRad;
            float wrappedPhase = std::fmod (lookupPhase, juce::MathConstants<float>::twoPi);
            if (wrappedPhase < 0.0f)
                wrappedPhase += juce::MathConstants<float>::twoPi;
                
            float rawSample = 0.0f;

            if (mode == 1) // Additive (8-Partial)
            {
                float additiveSum = 0.0f;
                float gainCompensation = 0.0f;

                for (int k = 1; k <= 8; ++k)
                {
                    float harmonicPhase = std::fmod (lookupPhase * k, juce::MathConstants<float>::twoPi);
                    if (harmonicPhase < 0.0f) harmonicPhase += juce::MathConstants<float>::twoPi;

                    float amplitude = 1.0f / static_cast<float> (k);
                    additiveSum += amplitude * std::sin (harmonicPhase);
                    gainCompensation += amplitude;
                }
                rawSample = additiveSum / gainCompensation;
                outputSample = std::tanh (rawSample + audioInputSum);
            }
            else // Wave Shapes (Mode 0)
            {
                switch (waveShape)
                {
                    case 0: rawSample = std::sin (wrappedPhase); break;
                    case 1: rawSample = 1.0f - 2.0f * std::abs (1.0f - (wrappedPhase / juce::MathConstants<float>::pi)); break;
                    case 2: rawSample = -1.0f + 2.0f * (wrappedPhase / juce::MathConstants<float>::twoPi); break;
                    case 3: rawSample = (wrappedPhase < juce::MathConstants<float>::pi) ? 1.0f : -1.0f; break;
                    case 4: rawSample = random.nextFloat() * 2.0f - 1.0f; break;
                    default: rawSample = std::sin (wrappedPhase); break;
                }

                // Wavefolding Logic
                float finalFoldDepth = juce::jlimit (0.0f, 1.0f, foldKnob + foldModOffset);
                if (finalFoldDepth > 0.001f)
                {
                    float drive = 1.0f + (finalFoldDepth * 5.0f);
                    float foldedSample = std::sin (rawSample * drive) * (1.0f / std::sqrt (drive));
                    outputSample = std::tanh (foldedSample + audioInputSum);
                }
                else
                {
                    outputSample = std::tanh (rawSample + audioInputSum);
                }
            }
        }
        
        return outputSample * envelope.getNextSample();
    }

    bool isActive() const { return envelope.isActive(); }

    juce::ADSR envelope;
    double phase = 0.0;
    double currentSampleRate = 44100.0;
    juce::Random random;
    
    SynthFilter internalFilter; 
};
