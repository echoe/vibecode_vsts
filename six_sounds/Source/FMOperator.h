// FMOperator.h
#pragma once
#include <JuceHeader.h>

struct FMOperator
{
    void prepare (double sampleRate)
    {
        currentSampleRate = sampleRate;
        envelope.setSampleRate (sampleRate);
        phase = 0.0;
        
        // Prepare JUCE SVF
        filter.prepare ({ sampleRate, 1, 1 });
        filter.reset();
        
        // Pre-allocate the comb filter buffer (1 second max delay)
        combBuffer.assign (static_cast<size_t> (sampleRate), 0.0f);
        combWriteIdx = 0;
    }

    void noteOn (const juce::ADSR::Parameters& envParams)
    {
        envelope.setParameters (envParams);
        envelope.noteOn();
        phase = 0.0;
        
        filter.reset();
        std::fill (combBuffer.begin(), combBuffer.end(), 0.0f);
        combWriteIdx = 0;
    }

    void noteOff()
    {
        envelope.noteOff();
    }

    void resetPhase (float phaseInDegrees)
    {
        phase = (phaseInDegrees / 360.0) * juce::MathConstants<double>::twoPi;
    }

    void resetVoiceState()
    {
        envelope.reset();
        filter.reset();
        std::fill (combBuffer.begin(), combBuffer.end(), 0.0f);
    }

    // Signature updated to accept separated mode/shape, internal audio routing, and filter params
    float processSample (double baseFrequency, float ratio, float detune, float phaseModulation,
                         float audioInput, int mode, int waveShape, int filterType, float filterQ)
    {
        if (!envelope.isActive()) return 0.0f;

        float outputSample = 0.0f;

        // --- MODE 2: FILTER ---
        if (mode == 2)
        {
            // Calculate cutoff using the base frequency modulated by the Ratio & Detune sliders
            float cutoff = static_cast<float> (baseFrequency * ratio) + detune;
            cutoff = juce::jlimit (20.0f, 20000.0f, cutoff);
            
            if (filterType == 3) // Comb Filter
            {
                float delayTimeMs = 1000.0f / cutoff; // Cutoff maps to delay time for pitch tracking
                int delaySamples = static_cast<int> ((delayTimeMs / 1000.0f) * currentSampleRate);
                delaySamples = juce::jlimit (1, (int)combBuffer.size() - 1, delaySamples);
                
                int readIdx = (combWriteIdx - delaySamples + (int)combBuffer.size()) % combBuffer.size();
                float delayedSample = combBuffer[static_cast<size_t> (readIdx)];
                
                // Map Q to feedback amount (0.0 to 0.95 max to prevent infinite buildup)
                float feedback = juce::jlimit (0.0f, 0.95f, filterQ / 10.0f);
                outputSample = audioInput + (delayedSample * feedback);
                
                combBuffer[static_cast<size_t> (combWriteIdx)] = outputSample;
                combWriteIdx = (combWriteIdx + 1) % combBuffer.size();
            }
            else // SVF (Lowpass, Highpass, Bandpass)
            {
                filter.setCutoffFrequency (cutoff);
                filter.setResonance (juce::jlimit (0.1f, 10.0f, filterQ));
                
                switch (filterType)
                {
                    case 0: filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass); break;
                    case 1: filter.setType (juce::dsp::StateVariableTPTFilterType::highpass); break;
                    case 2: filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass); break;
                    default: filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass); break;
                }
                
                outputSample = filter.processSample (1, audioInput);
            }
        }
        // --- MODE 0 & 1: STANDARD WAVE & ADDITIVE ---
        else
        {
            double freq = (baseFrequency * ratio) + detune;
            double phaseIncrement = (freq * juce::MathConstants<double>::twoPi) / currentSampleRate;
            
            phase += phaseIncrement;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        
            float lookupPhase = static_cast<float> (phase) + phaseModulation;
            float wrappedPhase = std::fmod (lookupPhase, juce::MathConstants<float>::twoPi);
            if (wrappedPhase < 0.0f)
                wrappedPhase += juce::MathConstants<float>::twoPi;
        
            if (mode == 1) // Additive (8-Partial)
            {
                float additiveSum = 0.0f;
                float gainCompensation = 0.0f;

                for (int k = 1; k <= 8; ++k)
                {
                    float harmonicPhase = std::fmod (lookupPhase * k, juce::MathConstants<float>::twoPi);
                    if (harmonicPhase < 0.0f)
                        harmonicPhase += juce::MathConstants<float>::twoPi;

                    float amplitude = 1.0f / static_cast<float> (k);
                    
                    additiveSum += amplitude * std::sin (harmonicPhase);
                    gainCompensation += amplitude;
                }

                outputSample = additiveSum / gainCompensation;
            }
            else // Wave Shapes (Mode 0)
            {
                switch (waveShape)
                {
                    case 0: // Sine
                        outputSample = std::sin (wrappedPhase);
                        break;
                        
                    case 1: // Triangle
                        outputSample = 1.0f - 2.0f * std::abs (1.0f - (wrappedPhase / juce::MathConstants<float>::pi));
                        break;
                        
                    case 2: // Saw
                        outputSample = -1.0f + 2.0f * (wrappedPhase / juce::MathConstants<float>::twoPi);
                        break;
                        
                    case 3: // Square
                        outputSample = (wrappedPhase < juce::MathConstants<float>::pi) ? 1.0f : -1.0f;
                        break;
                        
                    default:
                        outputSample = std::sin (wrappedPhase);
                        break;
                }
            }
        }
        
        // VCA processing applied to whichever mode generated/processed the sound
        return outputSample * envelope.getNextSample();
    }

    bool isActive() const { return envelope.isActive(); }

    juce::ADSR envelope;
    double phase = 0.0;
    double currentSampleRate = 44100.0;
    
    juce::dsp::StateVariableTPTFilter<float> filter;
    std::vector<float> combBuffer;
    int combWriteIdx = 0;
};
