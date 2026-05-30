// SynthFilter.h
#pragma once
#include <JuceHeader.h>
#include <cmath>

class SynthFilter
{
public:
    enum FilterType { Lowpass = 0, Highpass, Bandpass, Comb };
    
    SynthFilter() { reset(); }
    virtual ~SynthFilter() = default; // Essential for safe polymorphic deletion

    virtual void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        updateCoefficients();
    }

    void setSampleRate (double newSampleRate) 
    { 
        prepare (newSampleRate); 
    }

    void setCutoff (float cutoffHz)
    {
        float maxCutoff = static_cast<float>(sampleRate) * 0.49f;
        targetCutoff = juce::jlimit (20.0f, juce::jmin (20000.0f, maxCutoff), cutoffHz);
        updateCoefficients();
    }

    void setResonance (float resonanceQ)
    {
        targetResonance = juce::jlimit (0.1f, 10.0f, resonanceQ);
        updateCoefficients();
    }

    void setType (int typeIndex)
    {
        currentType = static_cast<FilterType>(juce::jlimit (0, 3, typeIndex));
    }

    float getPrecalculatedK() const noexcept
    {
        return 1.0f / targetResonance;
    }

    virtual void reset()
    {
        s1 = 0.0f;
        s2 = 0.0f;
    }

    virtual void noteStarted() {} // so keytracking works for comb filter

    // Standard Block Rate / Parameter Rate SVF Processing
    virtual float processSample (float input)
    {
        float hp = (input - (g + k) * s1 - s2) * h;
        float bp = g * hp + s1;
        float lp = g * bp + s2;

        s1 = 2.0f * bp - s1;
        s2 = 2.0f * lp - s2;

        if (std::isnan(s1) || std::isnan(s2) || std::isinf(s1) || std::isinf(s2))
        {
            reset(); // Instantly clears registers to prevent silent death traps
            return 0.0f;
        }

        switch (currentType)
        {
            case Highpass: return hp;
            case Bandpass: return bp;
            case Lowpass:  
            default:       return lp;
        }
    }

    // Fast Audio-Rate modulated SVF processing using Padé Approximant
    virtual float processSampleAudioRate (float input, float cutoffHz, float precalculatedK)
    {
        float x = 3.1415926535f * cutoffHz / static_cast<float>(sampleRate);
        float x2 = x * x;
        
        // Branchless 5th-order Padé approximant (Ultra-low CPU overhead)
        float g_mod = x * (945.0f - 105.0f * x2 + x2 * x2) / (945.0f - 420.0f * x2 + 15.0f * x2 * x2);
        float h_mod = 1.0f / (1.0f + g_mod * (g_mod + precalculatedK));
    
        float hp = (input - (g_mod + precalculatedK) * s1 - s2) * h_mod;
        float bp = g_mod * hp + s1;
        float lp = g_mod * bp + s2;
    
        s1 = 2.0f * bp - s1;
        s2 = 2.0f * lp - s2;
    
        if (std::isnan(s1) || std::isinf(s1)) 
        { 
            reset(); 
            return 0.0f; 
        }
    
        switch (currentType)
        {
            case Highpass: return hp;
            case Bandpass: return bp;
            case Lowpass:  
            default:       return lp;
        }
    } 

    // Virtual hook for Physical Modeling extensions
    virtual float processSampleComb (float input, float freqHz, float feedback, float dampingNormalized)
    {
        juce::ignoreUnused (freqHz, feedback, dampingNormalized);
        return input;
    }
        
    int getCurrentType() const noexcept { return static_cast<int>(currentType); }

protected:
    void updateCoefficients()
    {
        g = std::tan (juce::MathConstants<float>::pi * targetCutoff / static_cast<float>(sampleRate));
        k = 1.0f / targetResonance;
        h = 1.0f / (1.0f + g * (g + k));
    }

    // Filter Coefficients & Internal State Registers
    float g { 0.0f };
    float k { 0.0f };
    float h { 0.0f };
    float s1 { 0.0f };
    float s2 { 0.0f };

    // Parameters Cache accessible by Subclasses
    double sampleRate { 44100.0 };
    float targetCutoff { 1000.0f };
    float targetResonance { 0.707f };
    FilterType currentType { Lowpass };
};
