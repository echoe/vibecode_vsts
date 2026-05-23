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
