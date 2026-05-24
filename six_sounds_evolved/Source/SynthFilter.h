#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

class SynthFilter
{
public:
    enum FilterType
    {
        Lowpass = 0,
        Highpass,
        Bandpass,
        Comb
    };

    SynthFilter() 
    { 
        reset(); 
    }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        
        // Allocate a circular buffer large enough to handle low frequencies (down to ~10 Hz)
        int requiredBufferSize = static_cast<int>(sampleRate / 10.0) + 2;
        combBuffer.assign (requiredBufferSize, 0.0f);
        writePtr = 0;

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

    void reset()
    {
        s1 = 0.0f; 
        s2 = 0.0f;
        if (!combBuffer.empty())
        {
            std::fill (combBuffer.begin(), combBuffer.end(), 0.0f);
        }
        writePtr = 0;
    }

    float processSample (float input)
    {
        if (currentType == Comb)
        {
            if (combBuffer.empty()) return input;

            // 1. Calculate how many samples back our delay line needs to read
            float delaySamples = static_cast<float>(sampleRate) / targetCutoff;
            float readPtr = static_cast<float>(writePtr) - delaySamples;
            if (readPtr < 0.0f) readPtr += static_cast<float>(combBuffer.size());

            // 2. Perform linear interpolation for flawless, pitch-zipper-free sweeps
            int idx1 = static_cast<int>(readPtr) % combBuffer.size();
            int idx2 = (idx1 + 1) % combBuffer.size();
            float frac = readPtr - std::floor(readPtr);
            float delayedSample = combBuffer[idx1] * (1.0f - frac) + combBuffer[idx2] * frac;

            // 3. Map filter resonance out to a safe feedback gain ceiling (0.0 to 0.96)
            float feedback = juce::jlimit (0.0f, 0.96f, (targetResonance - 0.1f) / 9.9f);
            float output = input + (delayedSample * feedback);

            // 4. Update the circular buffer memory track
            combBuffer[writePtr] = output;
            writePtr = (writePtr + 1) % combBuffer.size();

            return output;
        }

        // --- Standard State Variable Filter Calculations ---
        float hp = (input - (g + k) * s1 - s2) * h;
        float bp = g * hp + s1;
        float lp = g * bp + s2;

        s1 = 2.0f * bp - s1;
        s2 = 2.0f * lp - s2;

        switch (currentType)
        {
            case Highpass: return hp;
            case Bandpass: return bp;
            case Lowpass:  
            default:       return lp;
        }
    }

private:
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

    // Comb Delay Data Structures
    std::vector<float> combBuffer;
    int writePtr { 0 };

    // Parameters Cache
    double sampleRate { 44100.0 };
    float targetCutoff { 1000.0f };
    float targetResonance { 0.707f };
    FilterType currentType { Lowpass };
};
