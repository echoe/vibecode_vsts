#pragma once
#include <JuceHeader.h>

class SynthFilter
{
public:
    SynthFilter() {}

    void prepare (double sampleRate)
    {
        currentSampleRate = sampleRate;
        iirFilter.reset();
        
        // Zero out the comb filter delay buffer
        combBuffer.fill (0.0f);
        writePtr = 0;
    }

    float processSample (float input, int mode, float frequency, float resonanceOrFeedback)
    {
        // Mode 0: Bypass
        if (mode == 0) return input;

        // 1. Handle Standard IIR Filters (Lowpass / Highpass)
        if (mode == 1 || mode == 2)
        {
            // Clamp values to safe boundaries to prevent coefficients from collapsing
            float safeFreq = juce::jlimit (20.0f, static_cast<float> (currentSampleRate * 0.49), frequency);
            float safeQ = juce::jmax (0.05f, resonanceOrFeedback * 5.0f); // Scale up resonance for audible Q peaks

            auto coefficients = (mode == 1) 
                ? juce::IIRCoefficients::makeLowPass (currentSampleRate, safeFreq, safeQ)
                : juce::IIRCoefficients::makeHighPass (currentSampleRate, safeFreq, safeQ);

            iirFilter.setCoefficients (coefficients);
            return iirFilter.processSingleSampleRaw (input);
        }

        // 2. Handle Feedback Comb Filter
        if (mode == 3)
        {
            // Calculate delay time in samples based on target frequency (Period = 1 / Freq)
            float delaySamples = static_cast<float> (currentSampleRate / juce::jlimit (10.0f, 4000.0f, frequency));
            
            // Safe feedback gain limit to prevent acoustic feedback explosions
            float fb = juce::jlimit (-0.98f, 0.98f, resonanceOrFeedback);

            // Read from delay line with fractional linear interpolation
            float readPtr = static_cast<float> (writePtr) - delaySamples;
            if (readPtr < 0.0f) readPtr += static_cast<float> (bufferSize);

            int idx1 = static_cast<int> (readPtr) % bufferSize;
            int idx2 = (idx1 + 1) % bufferSize;
            float frac = readPtr - std::floor (readPtr);

            float delayedSample = combBuffer[idx1] * (1.0f - frac) + combBuffer[idx2] * frac;

            // Feedback loop equation
            float output = input + (delayedSample * fb);

            // Write back to circular memory tape
            combBuffer[writePtr] = output;
            writePtr = (writePtr + 1) % bufferSize;

            return output;
        }

        return input;
    }

private:
    double currentSampleRate = 44100.0;
    juce::IIRFilter iirFilter;

    // Comb Filter Delay Components
    static constexpr int bufferSize = 8192;
    std::array<float, bufferSize> combBuffer { 0.0f };
    int writePtr = 0;
};
