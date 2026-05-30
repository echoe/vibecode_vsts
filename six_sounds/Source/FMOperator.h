// FMOperator.h
#pragma once
#include <JuceHeader.h>
#include "CombFilter.h" // Include your custom class

struct FMOperator
{
    void prepare (double sampleRate)
    {
        currentSampleRate = sampleRate;
        envelope.setSampleRate (sampleRate);
        phase = 0.0;        
        
        // Prepare SVF Filter
        filter.prepare ({ sampleRate, 1, 1 });
        filter.reset();
        
        // PREPARE COMB FILTER CLASS
        myCombFilter.prepare (sampleRate);
    }
    
    void noteOn (const juce::ADSR::Parameters& envParams)
    {
        envelope.setParameters (envParams);
        envelope.noteOn();
        phase = 0.0;
        
        // Reset both filters to prevent ghost echoes
        filter.reset();
        myCombFilter.reset();
    }
    
    void noteOff() { envelope.noteOff(); }
    
    void resetPhase (float phaseInDegrees) 
    { 
        phase = (phaseInDegrees / 360.0) * juce::MathConstants<double>::twoPi; 
    }
    
    void resetVoiceState() 
    {
        envelope.reset();
        filter.reset();
        myCombFilter.reset(); // Clear Comb Filter on voice reset
    }

    float processSample (double baseFrequency, float ratio, float detune, float phaseModulation,
                         float audioInput, int mode, int waveShape, int filterType, float filterQ)
    {
        if (!envelope.isActive()) return 0.0f;
        
        float outputSample = 0.0f;
        
        // --- MODE 2: FILTER ---
        if (mode == 2)
        {
            float cutoff = static_cast<float> (baseFrequency * ratio) + detune;
            cutoff = juce::jlimit (20.0f, 20000.0f, cutoff);
	    if (filterType == 3) // Comb Filter
            {
                // Opsix-style Bipolar mapping:
                // Assuming your Q knob goes from 0.0 to 10.0:
                // Q = 5.0 -> 0 feedback (dry)
                // Q > 5.0 -> Positive feedback (up to +0.99)
                // Q < 5.0 -> Negative feedback (down to -0.99)
                float mappedFeedback = ((filterQ / 5.0f) - 1.0f) * 0.99f;

                // Route audio directly into the upgraded CombFilter
		outputSample = myCombFilter.processSampleComb (audioInput, cutoff, mappedFeedback, 0.0f);
		static int debugCounter = 0;
                if (++debugCounter % 22050 == 0) // Prints twice a second
                {
                        DBG("Comb Executing! Input: " << audioInput << " | Cutoff: " << cutoff << " | Env: " << envelope.isActive());
                }
            }
            //if (filterType == 3) // Comb Filter - commented out to try this new option
            //{
                // Map Q to feedback amount (0.0 to 0.95 max)
                //float feedback = juce::jlimit (0.0f, 0.95f, filterQ / 10.0f);
                
                // Fixed 10% damping for a slight analog high-frequency decay
                //float damping = 0.1f; 
                
                // Route audio directly into your CombFilter class
                //outputSample = myCombFilter.processSampleComb (audioInput, cutoff, feedback, damping);
            //}
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
                    case 4: // White Noise
                        outputSample = random.nextFloat() * 2.0f - 1.0f;
                        break;
                    default: // Sine
                        outputSample = std::sin (wrappedPhase);
                        break;
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
    
    juce::dsp::StateVariableTPTFilter<float> filter;
    
    // The newly initialized Comb Filter instance
    CombFilter myCombFilter; 
};
