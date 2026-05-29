// SynthFilter.h
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

    float getPrecalculatedK() const noexcept
    {
        // Returns the damping coefficient: k = 1 / Q
        return 1.0f / targetResonance;
    }

    void reset()
    {
        s1 = 0.0f;
        s2 = 0.0f;
        lastCombDamping = 0.0f; // <-- Add this
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

            // Inside SynthFilter::processSample (Comb Section)
            float delaySamples = static_cast<float>(sampleRate) / targetCutoff;            
            // Safety Guard: Don't let delay exceed buffer size minus safety padding
            float maxDelay = static_cast<float>(combBuffer.size() - 2);
            delaySamples = juce::jlimit(1.0f, maxDelay, delaySamples);
            float readPtr = static_cast<float>(writePtr) - delaySamples;
            while (readPtr < 0.0f) 
            {
                readPtr += static_cast<float>(combBuffer.size());
            }

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

        if (std::isnan(s1) || std::isnan(s2) || std::isinf(s1) || std::isinf(s2))
        {
            reset(); // Instantly clears the registers and prevents the silent death trap
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
    // have a less accurate option for processing at audio rate
    float processSampleAudioRate (float input, float cutoffHz, float precalculatedK)
    {
        if (currentType == Comb)
        {
            // Fall back to your comb processing if needed, 
            // though comb filters require different delay modulation math.
            return processSample(input); 
        }
    
        // 1. Fast Tan Approximation for 'g'
        // x = pi * fc / fs
        float x = 3.1415926535f * cutoffHz / static_cast<float>(sampleRate);
        float x2 = x * x;
        
        // Branchless 5th-order Padé approximant (Ultra-low CPU overhead)
        float g_mod = x * (945.0f - 105.0f * x2 + x2 * x2) / (945.0f - 420.0f * x2 + 15.0f * x2 * x2);
    
        // 2. Only compute 'h' per sample. 'k' is passed in pre-calculated!
        float h_mod = 1.0f / (1.0f + g_mod * (g_mod + precalculatedK));
    
        // 3. Core SVF Equation Loop
        float hp = (input - (g_mod + precalculatedK) * s1 - s2) * h_mod;
        float bp = g_mod * hp + s1;
        float lp = g_mod * bp + s2;
    
        s1 = 2.0f * bp - s1;
        s2 = 2.0f * lp - s2;
    
        // Hard fallback safety net against NaN explosion
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
    // --- NEW: Audio-Rate Comb Filter for Physical Modeling ---
    float processSampleComb (float input, float freqHz, float feedback, float dampingNormalized)
        {
        if (combBuffer.empty()) return input;

        // 1. Calculate delay in samples safely, bounded by buffer size
        float delaySamples = static_cast<float>(sampleRate) / juce::jmax(10.0f, freqHz);
        float maxDelay = static_cast<float>(combBuffer.size() - 2);
        delaySamples = juce::jlimit (1.0f, maxDelay, delaySamples);

        float readPtr = static_cast<float>(writePtr) - delaySamples;
        while (readPtr < 0.0f) readPtr += static_cast<float>(combBuffer.size());

        // 2. Perform linear interpolation for pitch-zipper-free sweeps
        int idx1 = static_cast<int>(readPtr) % combBuffer.size();
        int idx2 = (idx1 + 1) % combBuffer.size();
        float frac = readPtr - std::floor(readPtr);
        float delayedSample = combBuffer[idx1] * (1.0f - frac) + combBuffer[idx2] * frac;

        // 3. Apply Damping (A simple 1-pole lowpass inside the feedback loop)
        // dampingNormalized acts as a smoothing coefficient. Higher = darker tail.
        lastCombDamping = (delayedSample * (1.0f - dampingNormalized)) + (lastCombDamping * dampingNormalized);

        // 4. Mix the feedback and soft-clip it to prevent infinite volume explosions
        float safeFeedback = juce::jlimit (0.0f, 0.995f, feedback);
        float output = input + (lastCombDamping * safeFeedback);
            
        combBuffer[writePtr] = std::tanh(output);
        writePtr = (writePtr + 1) % combBuffer.size();

        return output;
    }
        
    // Add a quick getter so the voice knows what mode the filter is in
    int getCurrentType() const noexcept { return static_cast<int>(currentType); }

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
    float lastCombDamping { 0.0f };
    FilterType currentType { Lowpass };
};
