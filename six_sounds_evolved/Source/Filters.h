#pragma once
#include <JuceHeader.h>

struct LightweightFilter
{
    float v0 = 0.0f, v1 = 0.0f; // Internal sample memory registers

    void reset() { v0 = 0.0f; v1 = 0.0f; }

    // Standard smooth 2-pole resonant lowpass filter processing loop
    float processSample (float input, float cutoffHz, float resonanceQ, double sampleRate)
    {
        // Sanity check constraints to prevent the filter from exploding
        cutoffHz = juce::jlimit (20.0f, 20000.0f, cutoffHz);
        resonanceQ = juce::jmax (0.1f, 10.0f, resonanceQ);

        // Calculate filter coefficients using basic biquad math
        float g = std::tan (juce::MathConstants<float>::pi * cutoffHz / static_cast<float>(sampleRate));
        float k = 1.0f / resonanceQ;
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;

        // Process state-variable steps
        float v2 = (input - v1 - g * v0) * a1;
        float lpf = v0 + g * v2;
        
        // Update history registers
        v0 = lpf + g * v2;
        v1 = v1 + 2.0f * g * v2;

        return lpf;
    }
};
