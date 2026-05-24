#pragma once
#include <JuceHeader.h>

struct FMOperator
{
    void prepare (double sampleRate)
    {
        currentSampleRate = sampleRate;
        envelope.setSampleRate (sampleRate);
        phase = 0.0;
    }

    void noteOn (const juce::ADSR::Parameters& envParams)
    {
        envelope.setParameters (envParams);
        envelope.noteOn();
        phase = 0.0;
    }

    void noteOff()
    {
        envelope.noteOff();
    }

    void resetPhase (float phaseInDegrees)
    {
        phase = (phaseInDegrees / 360.0) * juce::MathConstants<double>::twoPi;
    }

    float processSample (double baseFrequency, float ratio, float detune, float phaseModulation, int waveType)
    {
        if (!envelope.isActive()) return 0.0f;
    
        double freq = (baseFrequency * ratio) + detune;
        double phaseIncrement = (freq * juce::MathConstants<double>::twoPi) / currentSampleRate;
        
        phase += phaseIncrement;
        if (phase >= juce::MathConstants<double>::twoPi) 
            phase -= juce::MathConstants<double>::twoPi;
    
        // Combine base phase and modulation input
        float lookupPhase = static_cast<float> (phase) + phaseModulation;
        // Wrap phase cleanly between 0 and 2*Pi so non-sine shapes modulate smoothly
        float wrappedPhase = std::fmod (lookupPhase, juce::MathConstants<float>::twoPi);
        if (wrappedPhase < 0.0f) 
            wrappedPhase += juce::MathConstants<float>::twoPi;
    
        float rawOscillator = 0.0f;
    
        switch (waveType)
        {
            case 0: // Sine
                rawOscillator = std::sin (wrappedPhase);
                break;
                
            case 1: // Triangle
                rawOscillator = 1.0f - 2.0f * std::abs (1.0f - (wrappedPhase / juce::MathConstants<float>::pi));
                break;
                
            case 2: // Saw
                rawOscillator = -1.0f + 2.0f * (wrappedPhase / juce::MathConstants<float>::twoPi);
                break;
                
            case 3: // Square
                rawOscillator = (wrappedPhase < juce::MathConstants<float>::pi) ? 1.0f : -1.0f;
                break;

	    case 4: // Additive (8-Harmonic Engine)
        {
            float additiveSum = 0.0f;
            float gainCompensation = 0.0f;

            for (int k = 1; k <= 8; ++k)
            {
                float harmonicPhase = std::fmod (lookupPhase * k, juce::MathConstants<float>::twoPi);
                if (harmonicPhase < 0.0f) 
                    harmonicPhase += juce::MathConstants<float>::twoPi;

                // 1/k amplitude curve (Harmonic 1 = 1.0, Harmonic 2 = 0.5, Harmonic 3 = 0.33, etc.)
                float amplitude = 1.0f / static_cast<float> (k);
                
                additiveSum += amplitude * std::sin (harmonicPhase);
                gainCompensation += amplitude;
            }

            // Normalize the sum against the amplitude weights to prevent clipping
            rawOscillator = additiveSum / gainCompensation;
            break;
        }
                
            default:
                rawOscillator = std::sin (wrappedPhase);
                break;
        }
        
        return rawOscillator * envelope.getNextSample();
    }

    bool isActive() const { return envelope.isActive(); }

    juce::ADSR envelope;
    double phase = 0.0;
    double currentSampleRate = 44100.0;
};
