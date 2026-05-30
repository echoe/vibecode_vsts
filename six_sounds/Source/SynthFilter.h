// SynthFilter.h
#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <vector>

struct Grain {
    float position;
    float phase;
    float durationInSamples; 
    bool  isActive;
};

class SynthFilter
{
public:
    enum FilterType { Lowpass = 0, Highpass, Bandpass, Comb, Granular };
    
    SynthFilter() 
    {
        grains.resize(maxGrains, {0.0f, 0.0f, 0.0f, false});
        reset(); 
    }
    virtual ~SynthFilter() = default; 

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        updateCoefficients();

        // 10 Hz requires a huge buffer. Added margin for Hermite/Scatter math.
        int requiredBufferSize = static_cast<int>(newSampleRate / 10.0) + 10;
        combBuffer.assign (static_cast<size_t>(requiredBufferSize), 0.0f);
        writePtr = 0;

        for (auto& g : grains) {
            g.isActive = false;
        }
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
        currentType = static_cast<FilterType>(juce::jlimit (0, 4, typeIndex));
    }

    float getPrecalculatedK() const noexcept
    {
        return 1.0f / targetResonance;
    }

    void reset()
    {
        s1 = 0.0f;
        s2 = 0.0f;
        lastCombDamping = 0.0f;
        
        if (!combBuffer.empty())
            std::fill (combBuffer.begin(), combBuffer.end(), 0.0f);
            
        writePtr = 0;

        for (auto& g : grains) {
            g.isActive = false;
        }
    }

    void noteStarted()
    {
        std::fill(combBuffer.begin(), combBuffer.end(), 0.0f);
        writePtr = 0;
        lastCombDamping = 0.0f;

        for (auto& g : grains) {
            g.isActive = false;
        }
    }

    // Standard Block Rate SVF
    virtual float processSample (float input)
    {
        float hp = (input - (g + k) * s1 - s2) * h;
        float bp = g * hp + s1;
        float lp = g * bp + s2;

        s1 = 2.0f * bp - s1;
        s2 = 2.0f * lp - s2;

        if (std::isnan(s1) || std::isnan(s2) || std::isinf(s1) || std::isinf(s2))
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

    // Fast Audio-Rate modulated SVF
    virtual float processSampleAudioRate (float input, float cutoffHz, float precalculatedK)
    {
        float x = 3.1415926535f * cutoffHz / static_cast<float>(sampleRate);
        float x2 = x * x;
        
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

    // Standard Comb Filter
    float processSampleComb (float input, float freqHz, float feedback, float dampingNormalized)
    {
        if (combBuffer.empty()) return input;
        int bufferSize = static_cast<int>(combBuffer.size());
        
        float delaySamples = static_cast<float>(sampleRate) / juce::jmax (10.0f, freqHz);
        float maxDelay = static_cast<float>(bufferSize - 4);
        delaySamples = juce::jlimit (1.0f, maxDelay, delaySamples);
        float readPtrVal = static_cast<float>(writePtr) - delaySamples;
        
        while (readPtrVal < 0.0f) readPtrVal += static_cast<float>(bufferSize);
        
        int idx1 = static_cast<int>(readPtrVal) % bufferSize;
        float frac = readPtrVal - std::floor (readPtrVal);
        int idx0 = (idx1 - 1 + bufferSize) % bufferSize;
        int idx2 = (idx1 + 1) % bufferSize;
        int idx3 = (idx1 + 2) % bufferSize;
        
        float y0 = combBuffer[static_cast<size_t>(idx0)];
        float y1 = combBuffer[static_cast<size_t>(idx1)];
        float y2 = combBuffer[static_cast<size_t>(idx2)];
        float y3 = combBuffer[static_cast<size_t>(idx3)];
        
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        float delayedSample = ((c3 * frac + c2) * frac + c1) * frac + c0;
        
        lastCombDamping = (delayedSample * (1.0f - dampingNormalized)) + (lastCombDamping * dampingNormalized);
        float safeFeedback = juce::jlimit (-0.995f, 0.995f, feedback);
        float output = input + (lastCombDamping * safeFeedback);
        
        combBuffer[static_cast<size_t>(writePtr)] = std::tanh (output);
        writePtr = (writePtr + 1) % bufferSize;
        
        return output;
    }

    // Granular Resonator / Glitch Filter
    float processSampleGranular (float input, float freqHz, float scatterAmount, float grainDurationMs, float feedback, float dampingNormalized)
    {
        if (combBuffer.empty()) return input;
        
        int bSize = static_cast<int>(combBuffer.size());
        float baseDelaySamples = static_cast<float>(sampleRate) / juce::jmax(10.0f, freqHz);
        float maxDelay = static_cast<float>(bSize - 4);
        
        float outputAccumulator = 0.0f;
        int activeGrainCount = 0;

        for (auto& grain : grains)
        {
            // Stochastic spawning
            if (!grain.isActive && fastRandom() < 0.02f) 
            {
                grain.isActive = true;
                grain.phase = 0.0f;
                
                // Set duration safely
                float durationSamples = (grainDurationMs / 1000.0f) * static_cast<float>(sampleRate);
                grain.durationInSamples = juce::jmax(10.0f, durationSamples);
                
                // Jitter position based on scatter parameter
                float jitter = (fastRandom() * 2.0f - 1.0f) * scatterAmount * static_cast<float>(sampleRate);
                float delaySamples = juce::jlimit(1.0f, maxDelay, baseDelaySamples + jitter);
                
                grain.position = static_cast<float>(writePtr) - delaySamples;
                while (grain.position < 0.0f) grain.position += static_cast<float>(bSize);
            }

            if (grain.isActive)
            {
                activeGrainCount++;
                
                // Moving read head for the grain
                float readPos = grain.position + grain.phase; 
                while (readPos >= bSize) readPos -= bSize;
                
                // Hermite Interpolation
                int idx1 = static_cast<int>(readPos) % bSize;
                float frac = readPos - std::floor(readPos);
                int idx0 = (idx1 - 1 + bSize) % bSize;
                int idx2 = (idx1 + 1) % bSize;
                int idx3 = (idx1 + 2) % bSize;
                
                float y0 = combBuffer[static_cast<size_t>(idx0)];
                float y1 = combBuffer[static_cast<size_t>(idx1)];
                float y2 = combBuffer[static_cast<size_t>(idx2)];
                float y3 = combBuffer[static_cast<size_t>(idx3)];
                
                float c0 = y1;
                float c1 = 0.5f * (y2 - y0);
                float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
                float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                float sample = ((c3 * frac + c2) * frac + c1) * frac + c0;
                
                // Hanning Window to prevent clicks
                float window = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * grain.phase / grain.durationInSamples));
                
                outputAccumulator += sample * window;
                
                // Advance phase
                grain.phase += 1.0f;
                if (grain.phase >= grain.durationInSamples)
                {
                    grain.isActive = false;
                }
            }
        }
        
        // Energy compensation to prevent loud spikes when many grains overlap
        if (activeGrainCount > 0)
        {
            outputAccumulator /= std::sqrt(static_cast<float>(activeGrainCount)); 
        }

        // Apply feedback and damping to write buffer
        lastCombDamping = (outputAccumulator * (1.0f - dampingNormalized)) + (lastCombDamping * dampingNormalized);
        float safeFeedback = juce::jlimit (-0.995f, 0.995f, feedback);
        float toBuffer = input + (lastCombDamping * safeFeedback);
        
        combBuffer[static_cast<size_t>(writePtr)] = std::tanh(toBuffer); // Soft-clip
        writePtr = (writePtr + 1) % bSize;
        
        return outputAccumulator + input; // Wet + Dry
    }

    int getCurrentType() const noexcept { return static_cast<int>(currentType); }

protected:
    void updateCoefficients()
    {
        g = std::tan (juce::MathConstants<float>::pi * targetCutoff / static_cast<float>(sampleRate));
        k = 1.0f / targetResonance;
        h = 1.0f / (1.0f + g * (g + k));
    }

    // Fast lock-free random generator (Linear Congruential Generator)
    float fastRandom() 
    {
        lcgState = 1664525 * lcgState + 1013904223;
        return static_cast<float>(lcgState) / static_cast<float>(0xFFFFFFFF);
    }

    // Filter Coefficients & Internal State Registers
    float g { 0.0f };
    float k { 0.0f };
    float h { 0.0f };
    float s1 { 0.0f };
    float s2 { 0.0f };

    // Comb & Granular Memory
    std::vector<float> combBuffer;
    int writePtr { 0 };
    float lastCombDamping { 0.0f };
    
    // Granular System
    static constexpr int maxGrains = 16; // Adjust based on CPU limits
    std::vector<Grain> grains;
    uint32_t lcgState { 12345 }; // Seed for fastRandom()

    // Parameters Cache
    double sampleRate { 44100.0 };
    float targetCutoff { 1000.0f };
    float targetResonance { 0.707f };
    FilterType currentType { Lowpass };
};
