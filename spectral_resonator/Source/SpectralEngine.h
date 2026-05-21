#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <complex>

class SpectralEngine
{
public:
    SpectralEngine() : mFft(mFftOrder) {}

    void prepare(double sampleRate)
    {
        mSampleRate = sampleRate;
        mFftSize = mFft.getSize();
        mHopSize = mFftSize / 4; // 4x overlap for smooth synthesis

        mInputBuffer.assign(mFftSize * 2, 0.0f);
        mOutputBuffer.assign(mFftSize * 2, 0.0f);
        mFftWorkspace.assign(mFftSize * 2, 0.0f);
        mPreviousMagnitudes.assign(mFftSize / 2 + 1, 0.0f);

        // Generate Hann window
        mWindow.resize(mFftSize);
        juce::dsp::WindowingFunction<float>::fillWindowingTables(
            mWindow.data(), mFftSize, 
            juce::dsp::WindowingFunction<float>::hann, false
        );

        mWriteIndex = 0;
        mReadIndex = 0;
        mHopCounter = 0;
    }

    void processBlock(float* channelData, int numSamples, float targetFreq, float resonance, float decay)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            // 1. Push sample into input ring buffer
            mInputBuffer[mWriteIndex] = channelData[i];

            // 2. Pull synthesis sample out from output ring buffer
            channelData[i] = mOutputBuffer[mReadIndex];
            mOutputBuffer[mReadIndex] = 0.0f; // Clear accumulated space

            mWriteIndex = (mWriteIndex + 1) % mInputBuffer.size();
            mReadIndex = (mReadIndex + 1) % mOutputBuffer.size();

            // 3. Time to process an FFT frame?
            if (++mHopCounter >= mHopSize)
            {
                mHopCounter = 0;
                performSpectralProcessing(targetFreq, resonance, decay);
            }
        }
    }

private:
    void performSpectralProcessing(float targetFreq, float resonance, float decay)
    {
        // Gather the last 'mFftSize' samples from input buffer and apply analysis window
        int readPtr = (mWriteIndex - mFftSize + mInputBuffer.size()) % mInputBuffer.size();
        for (int i = 0; i < mFftSize; ++i)
        {
            mFftWorkspace[i] = mInputBuffer[readPtr] * mWindow[i];
            readPtr = (readPtr + 1) % mInputBuffer.size();
        }

        // Pad remaining workspace with zeros for JUCE complex FFT requirements
        std::fill(mFftWorkspace.begin() + mFftSize, mFftWorkspace.end(), 0.0f);

        // Forward FFT: Converts time domain data into interleaved Complex data in-place
        mFft.performRealOnlyForwardTransform(mFftWorkspace.data());

        auto* complexBins = reinterpret_cast<std::complex<float>*>(mFftWorkspace.data());
        int numBins = mFftSize / 2 + 1;
        float binWidthHz = static_cast<float>(mSampleRate) / static_cast<float>(mFftSize);
        int targetBin = std::round(targetFreq / binWidthHz);

        // Process spectral bins
        for (int bin = 0; bin < numBins; ++bin)
        {
            float magnitude = std::abs(complexBins[bin]);
            float phase = std::arg(complexBins[bin]);

            if (bin == targetBin && targetBin < numBins)
            {
                // Feedback loop: Inject lingering spectral energy
                magnitude += mPreviousMagnitudes[bin] * decay * resonance;
                magnitude *= (1.0f + resonance * 12.0f);
            }
            else
            {
                // Attenuate unrelated frequencies to isolate the resonance
                magnitude *= (1.0f - resonance * 0.4f);
            }

            mPreviousMagnitudes[bin] = magnitude;
            complexBins[bin] = std::polar(magnitude, phase);
        }

        // Reconstruct negative frequencies for symmetric Real IFFT
        for (int bin = 1; bin < mFftSize / 2; ++bin)
        {
            complexBins[mFftSize - bin] = std::conj(complexBins[bin]);
        }

        // Inverse FFT: Converts back to time domain data in-place
        mFft.performRealOnlyInverseTransform(mFftWorkspace.data());

        // Overlap-Add synthesis back into the output buffer
        int writePtr = mReadIndex;
        for (int i = 0; i < mFftSize; ++i)
        {
            mOutputBuffer[writePtr] += mFftWorkspace[i] * mWindow[i];
            writePtr = (writePtr + 1) % mOutputBuffer.size();
        }
    }

    static constexpr int mFftOrder = 11; // 2^11 = 2048 points
    juce::dsp::FFT mFft;
    double mSampleRate = 44100.0;
    int mFftSize;
    int mHopSize;
    int mHopCounter;
    int mWriteIndex;
    int mReadIndex;

    std::vector<float> mInputBuffer;
    std::vector<float> mOutputBuffer;
    std::vector<float> mFftWorkspace;
    std::vector<float> mWindow;
    std::vector<float> mPreviousMagnitudes;
};
