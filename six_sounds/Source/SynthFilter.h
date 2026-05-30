// SynthFilter.h
#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <cstdlib>

struct Grain {
    float position;
    float phase;
    float durationInSamples; // Calculated from knob
    bool  isActive;
};

class SynthFilter
{
public:
    enum FilterType { Lowpass = 0, Highpass, Bandpass, Comb };
    
    SynthFilter() { reset(); }
    virtual ~SynthFilter() = default; // Essential for safe polymorphic deletion

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        updateCoefficients();
	// CombFilter prep
	// 10 Hz requires a huge buffer. Let's add +10 just to be incredibly safe for the Hermite math.
        int requiredBufferSize = static_cast<int>(newSampleRate / 10.0) + 10;
        combBuffer.assign (static_cast<size_t>(requiredBufferSize), 0.0f);
        writePtr = 0;

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

    void reset() // combined to work
    {
        s1 = 0.0f;
        s2 = 0.0f;
        lastCombDamping = 0.0f;
        if (!combBuffer.empty())
        {
            std::fill (combBuffer.begin(), combBuffer.end(), 0.0f);
        }
        writePtr = 0;
    }

    void noteStarted() //reset the combbuffer for a new note so keytracking works
    {
        std::fill(combBuffer.begin(), combBuffer.end(), 0.0f);
        writePtr = 0;
        lastCombDamping = 0.0f;
    }


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

    float processSampleComb (float input, float freqHz, float feedback, float dampingNormalized)
    {
        if (combBuffer.empty()) return input;
        int bufferSize = static_cast<int>(combBuffer.size());
        // Pitch to delay time
        float delaySamples = static_cast<float>(sampleRate) / juce::jmax (10.0f, freqHz);
        // We need a margin of 4 samples for Hermite interpolation safety
        float maxDelay = static_cast<float>(bufferSize - 4);
        delaySamples = juce::jlimit (1.0f, maxDelay, delaySamples);
	float readPtrVal = static_cast<float>(writePtr) - delaySamples;
        while (readPtrVal < 0.0f)
        {
            readPtrVal += static_cast<float>(bufferSize);
        }
        // --- 4-POINT HERMITE INTERPOLATION ---
        int idx1 = static_cast<int>(readPtrVal) % bufferSize;
        float frac = readPtrVal - std::floor (readPtrVal);
        int idx0 = (idx1 - 1 + bufferSize) % bufferSize;
        int idx2 = (idx1 + 1) % bufferSize;
        int idx3 = (idx1 + 2) % bufferSize;
        float y0 = combBuffer[static_cast<size_t>(idx0)];
        float y1 = combBuffer[static_cast<size_t>(idx1)];
        float y2 = combBuffer[static_cast<size_t>(idx2)];
        float y3 = combBuffer[static_cast<size_t>(idx3)];
        // Hermite polynomial math (Preserves high frequencies)
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        float delayedSample = ((c3 * frac + c2) * frac + c1) * frac + c0;
        // 1-pole lowpass inside the feedback loop
        lastCombDamping = (delayedSample * (1.0f - dampingNormalized)) + (lastCombDamping * dampingNormalized);
        float safeFeedback = juce::jlimit (-0.995f, 0.995f, feedback);
        float output = input + (lastCombDamping * safeFeedback);
        // Soft-clip the buffer input so it screams without blowing up your speakers
        combBuffer[static_cast<size_t>(writePtr)] = std::tanh (output);
        writePtr = (writePtr + 1) % bufferSize;
        return output;
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

    // --- Delay Line Variables ---
    int bufferSize { 44100 }; // Must be an int!
    int writeIndex { 0 };     // Must be an int!
    float* delayBuffer = nullptr; 

    // Comb
    std::vector<float> combBuffer;
    int writePtr { 0 };
    float lastCombDamping { 0.0f };

    // Parameters Cache
    double sampleRate { 44100.0 };
    float targetCutoff { 1000.0f };
    float targetResonance { 0.707f };
    FilterType currentType { Lowpass };
};
