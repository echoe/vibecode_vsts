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
        // Set the ceiling just below absolute clipping (~ -0.2 dB)
        // const float ceiling = 0.98f; 
        
        // 100ms release time to smoothly return the volume to normal
        const float releaseTimeSeconds = 0.100f;
        const float releaseCoef = std::exp (-1.0f / (currentSampleRate * releaseTimeSeconds));

        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Find the absolute highest peak across both the left and right channels
            float peak = 0.0f;
            for (int chan = 0; chan < numChannels; ++chan)
            {
                peak = std::max (peak, std::abs (buffer.getSample (chan, sample)));
            }

            // Instant-attack envelope follower
            if (peak > envelopeTracker)
            {
                envelopeTracker = peak;
            }
            else
            {
                // Smooth exponential decay back down
                envelopeTracker = peak + releaseCoef * (envelopeTracker - peak);
            }

            // If the signal pushes past our safety ceiling, calculate the scaling factor
            float attenuationGain = 1.0f;
            if (envelopeTracker > ceilingLinear)
            {
                attenuationGain = ceilingLinear / envelopeTracker;
            }

            // Attenuate all channels uniformly to preserve the stereo image
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
