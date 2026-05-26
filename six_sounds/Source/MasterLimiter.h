// MasterLimiter.h
#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <algorithm>

class MasterLimiter
{
public:
    MasterLimiter() {}

    void prepare (double sampleRate)
    {
        currentSampleRate = sampleRate;
        envelopeTracker = 0.0f;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, float ceilingLinear)
    {
        // Safety check: if ceiling is invalid or corrupted, default to a safe value
        if (std::isnan(ceilingLinear) || std::isinf(ceilingLinear) || ceilingLinear <= 0.0f)
            ceilingLinear = 0.95f; 
    
        const float releaseTimeSeconds = 0.100f;
        const float releaseCoef = std::exp (-1.0f / (currentSampleRate * releaseTimeSeconds));
    
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
    
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float peak = 0.0f;
            
            for (int chan = 0; chan < numChannels; ++chan)
            {
                float s = buffer.getSample (chan, sample);
    
                // 1. SANITIZE INPUT: Catch NaN/Inf from an exploded synth engine instantly
                if (std::isnan(s) || std::isinf(s))
                {
                    buffer.setSample(chan, sample, 0.0f);
                    s = 0.0f;
                }
    
                peak = std::max (peak, std::abs (s));
            }
    
            // 2. SANITIZE TRACKER: If the tracker somehow became NaN, force recover it
            if (std::isnan(envelopeTracker) || std::isinf(envelopeTracker))
                envelopeTracker = 0.0f;
    
            // Instant-attack envelope follower
            if (peak > envelopeTracker)
            {
                envelopeTracker = peak;
            }
            else
            {
                envelopeTracker = peak + releaseCoef * (envelopeTracker - peak);
            }
    
            // Calculate scaling factor
            float attenuationGain = 1.0f;
            if (envelopeTracker > ceilingLinear)
            {
                attenuationGain = ceilingLinear / envelopeTracker;
            }
    
            // 3. SANITIZE GAIN: Ensure attenuation factor is a valid number
            if (std::isnan(attenuationGain) || std::isinf(attenuationGain))
                attenuationGain = 0.0f;
    
            // Attenuate all channels uniformly
            for (int chan = 0; chan < numChannels; ++chan)
            {
                float nativeSample = buffer.getSample (chan, sample);
                buffer.setSample (chan, sample, nativeSample * attenuationGain);
            }
        }
    }

private:
    double currentSampleRate = 44100.0;
    float envelopeTracker = 0.0f;
};
