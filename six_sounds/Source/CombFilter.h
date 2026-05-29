// CombFilter.h
#pragma once
#include "SynthFilter.h"
#include <vector>

class CombFilter : public SynthFilter
{
public:
    CombFilter()
    {
        CombFilter::reset();
    }

    void prepare (double newSampleRate) override
    {
        // First setup base SVF parameters securely
        SynthFilter::prepare (newSampleRate);
        
        // Allocate circular buffer tracking down to ~10 Hz
        int requiredBufferSize = static_cast<int>(sampleRate / 10.0) + 2;
        combBuffer.assign (requiredBufferSize, 0.0f);
        writePtr = 0;
    }

    void reset() override
    {
        SynthFilter::reset(); // Clear SVF registers
        
        lastCombDamping = 0.0f;
        if (!combBuffer.empty())
        {
            std::fill (combBuffer.begin(), combBuffer.end(), 0.0f);
        }
        writePtr = 0;
    }

    // Overridden standard processing utilizing custom delay logic
    float processSample (float input) override
    {
        if (combBuffer.empty()) return input;

        float delaySamples = static_cast<float>(sampleRate) / targetCutoff;            
        float maxDelay = static_cast<float>(combBuffer.size() - 2);
        delaySamples = juce::jlimit (1.0f, maxDelay, delaySamples);
        
        float readPtrVal = static_cast<float>(writePtr) - delaySamples;
        while (readPtrVal < 0.0f) 
        {
            readPtrVal += static_cast<float>(combBuffer.size());
        }

        // Linear interpolation for clean sweeps
        int idx1 = static_cast<int>(readPtrVal) % combBuffer.size();
        int idx2 = (idx1 + 1) % combBuffer.size();
        float frac = readPtrVal - std::floor (readPtrVal);
        float delayedSample = combBuffer[idx1] * (1.0f - frac) + combBuffer[idx2] * frac;

        // Map resonance parameter out to safe feedback ceilings (0.0 to 0.96)
        float feedback = juce::jlimit (0.0f, 0.96f, (targetResonance - 0.1f) / 9.9f);
        float output = input + (delayedSample * feedback);

        // Commit to circular buffer step tracking
        combBuffer[writePtr] = output;
        writePtr = (writePtr + 1) % combBuffer.size();

        return output;
    }

    // Fall back smoothly to local standard process block if sample modulation runs here
    float processSampleAudioRate (float input, float cutoffHz, float precalculatedK) override
    {
        juce::ignoreUnused (cutoffHz, precalculatedK);
        return processSample (input); 
    }    

    // Dedicated Physical Modeling Processing Call
    float processSampleComb (float input, float freqHz, float feedback, float dampingNormalized) override
    {
        if (combBuffer.empty()) return input;

        float delaySamples = static_cast<float>(sampleRate) / juce::jmax (10.0f, freqHz);
        float maxDelay = static_cast<float>(combBuffer.size() - 2);
        delaySamples = juce::jlimit (1.0f, maxDelay, delaySamples);

        float readPtrVal = static_cast<float>(writePtr) - delaySamples;
        while (readPtrVal < 0.0f) readPtrVal += static_cast<float>(combBuffer.size());

        int idx1 = static_cast<int>(readPtrVal) % combBuffer.size();
        int idx2 = (idx1 + 1) % combBuffer.size();
        float frac = readPtrVal - std::floor (readPtrVal);
        float delayedSample = combBuffer[idx1] * (1.0f - frac) + combBuffer[idx2] * frac;

        // Damping via 1-pole lowpass inside feedback loop
        lastCombDamping = (delayedSample * (1.0f - dampingNormalized)) + (lastCombDamping * dampingNormalized);

        // Mix feedback and soft-clip with tanh to eliminate volume explosions
        float safeFeedback = juce::jlimit (0.0f, 0.995f, feedback);
        float output = input + (lastCombDamping * safeFeedback);
            
        combBuffer[writePtr] = std::tanh (output);
        writePtr = (writePtr + 1) % combBuffer.size();

        return output;
    }

private:
    std::vector<float> combBuffer;
    int writePtr { 0 };
    float lastCombDamping { 0.0f };
};
