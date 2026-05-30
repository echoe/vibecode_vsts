// CombFilter.h
#pragma once
#include "SynthFilter.h"
#include <vector>
#include <cmath>

class CombFilter : public SynthFilter
{
public:
    CombFilter()
    {
        CombFilter::reset();
    }

    void prepare (double newSampleRate) override
    {
        SynthFilter::prepare (newSampleRate);

        // Use newSampleRate explicitly!
        // 10 Hz requires a huge buffer. Let's add +10 just to be incredibly safe for the Hermite math.
        int requiredBufferSize = static_cast<int>(newSampleRate / 10.0) + 10; 
        
        combBuffer.assign (static_cast<size_t>(requiredBufferSize), 0.0f);
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

    // removed processSample and processSampleAudioRate so that SynthFilter picks that up if we don't call processSampleComb
    // Dedicated Physical Modeling Processing Call (Opsix Style)
    float processSampleComb (float input, float freqHz, float feedback, float dampingNormalized) override
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

        // --- Damping ---
        // 1-pole lowpass inside the feedback loop
        lastCombDamping = (delayedSample * (1.0f - dampingNormalized)) + (lastCombDamping * dampingNormalized);

        // --- Bipolar Feedback ---
        // Scale your 0.0 -> 1.0 envelope attack to a bipolar -0.99 to +0.99 range
        // If Attack knob is at 0.5, feedback is 0.
        // If Attack knob is at 1.0, feedback is +0.99.
        float mappedFeedback = (feedback - 0.5f) * 1.98f;
        float safeFeedback = juce::jlimit (-0.995f, 0.995f, mappedFeedback);
        
        float output = input + (lastCombDamping * safeFeedback);

        // Soft-clip the buffer input so it screams without blowing up your speakers
        combBuffer[static_cast<size_t>(writePtr)] = std::tanh (output);
        writePtr = (writePtr + 1) % bufferSize;

        return output;
    }

private:
    std::vector<float> combBuffer;
    int writePtr { 0 };
    float lastCombDamping { 0.0f };
};
