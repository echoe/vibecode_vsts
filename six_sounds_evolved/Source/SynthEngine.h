#pragma once
#include <JuceHeader.h>

constexpr int numOps = 4;
enum WaveType { SoundWave = 1, AdditiveWave, Resonator, Filter };

// Thread-safe circular buffer for the UI Oscilloscope
class AudioScopeBuffer {
public:
    AudioScopeBuffer() { buffer.fill(0.0f); }
    
    void pushSample(float sample) noexcept {
        buffer[writeIndex.load()] = sample;
        writeIndex = (writeIndex.load() + 1) % buffer.size();
    }
    
    void readSamples(std::array<float, 256>& dest) noexcept {
        int readIdx = (writeIndex.load() - 256 + buffer.size()) % buffer.size();
        for (int i = 0; i < 256; ++i) {
            dest[i] = buffer[(readIdx + i) % buffer.size()];
        }
    }
private:
    std::array<float, 1024> buffer;
    std::atomic<int> writeIndex{ 0 };
};

// Represents a singular multi-mode Operator/Operator Engine
struct Operator {
    WaveType currentType = SoundWave;
    juce::ADSR adsr;
    float phase = 0.0f;
    float phaseIncrement = 0.0f;
    float filterState = 0.0f; // Simple one-pole state
    
    // Parameters cached from APVTS per block
    int waveSel = 1;
    int soundType = 1;
    float ratio = 1.0f;
    float pulseWidth = 0.5f;
    int partials[8] = {1,1,1,1,1,1,1,1};
    int resKey = 4; // Default C
    float resQ = 50.0f;
    float resDamping = 50.0f;
    int filtType = 1;
    float filtRes = 0.0f;
    float filtCutoff = 100.0f;
    float outputLevel = 1.0f;

    void updatePhaseInc(double sampleRate, float frequency) {
        phaseIncrement = (frequency * ratio) / static_cast<float>(sampleRate);
    }

    float process(float inputModulation, float incomingChainAudio) {
        float sample = 0.0f;
        phase += phaseIncrement + inputModulation;
        if (phase >= 1.0f) phase -= 1.0f;
        if (phase < 0.0f) phase += 1.0f;

        switch (waveSel) {
            case 1: // Sound Wave
                if (soundType == 1) sample = std::sin(phase * juce::MathConstants<float>::twoPi); // Sine
                else if (soundType == 2) sample = (phase * 2.0f) - 1.0f; // Saw
                else if (soundType == 3 || soundType == 4) sample = (phase < pulseWidth) ? 1.0f : -1.0f; // Pulse / Square
                break;

            case 2: // Additive Wave (Blocky combination of 8 partial multipliers)
                for (int i = 0; i < 8; ++i) {
                    sample += std::sin(phase * juce::MathConstants<float>::twoPi * partials[i]) * 0.125f;
                }
                break;

            case 3: { // Resonator Emulation (Comb / Highly tuned feedback bandpass)
                float targetFreq = 440.0f * std::pow(2.0f, (resKey - 9) / 12.0f);
                float dampingFactor = 1.0f - (resDamping / 100.0f) * 0.05f;
                filterState = filterState * dampingFactor + (incomingChainAudio * (resQ / 100.0f));
                sample = filterState;
                break;
            }
            case 4: { // Dynamic Filter Engine
                float rc = 1.0f / (juce::MathConstants<float>::twoPi * (filtCutoff / 100.0f) * 20000.0f);
                float alpha = 1.0f / (rc + 1.0f);
                filterState = filterState + alpha * (incomingChainAudio - filterState); // Simple LP baseline
                sample = (filtType == 1 || filtType == 2) ? filterState : (incomingChainAudio - filterState);
                break;
            }
        }
        return sample * adsr.getNextSample() * outputLevel;
    }
};

struct LFO {
    float phase = 0.0f;
    int target = 0;
    float rate = 1.0f;
    int type = 1;

    float getSample(double sampleRate) {
        phase += rate / static_cast<float>(sampleRate);
        if (phase >= 1.0f) phase -= 1.0f;
        
        if (type == 1) return std::sin(phase * juce::MathConstants<float>::twoPi);
        if (type == 2) return (phase * 2.0f) - 1.0f;
        return (phase < 0.5f) ? 1.0f : -1.0f;
    }
};

class SynthSound : public juce::SynthesiserSound {
public:
    // Update these two methods to match JUCE's native virtual signatures:
    bool appliesToNote (int midiNoteNumber) override { return true; }
    bool appliesToChannel (int midiChannel) override { return true; }
};

class SynthVoice : public juce::SynthesiserVoice {
public:
    SynthVoice() {
        for (auto& op : operators) op.adsr.setSampleRate(44100.0);
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override { return dynamic_cast<SynthSound*>(sound) != nullptr; }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override {
        currentFreq = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        for (auto& op : operators) op.adsr.noteOn();
        level = velocity;
    }
    
    void stopNote(float, bool allowTailOff) override {
        for (auto& op : operators) op.adsr.noteOff();
        if (!allowTailOff) clearCurrentNote();
    }
    
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override {
        // Voice rendering context handles processing across operators
        bool active = false;
        for (auto& op : operators) { if (op.adsr.isActive()) active = true; }
        if (!active) { clearCurrentNote(); return; }

        for (int sample = 0; sample < numSamples; ++sample) {
            std::array<float, numOps> opOutputs{ 0.0f };
            float mixedSample = 0.0f;

            // Compute sequential operations + FM Matrix routing calculations
            for (int i = 0; i < numOps; ++i) {
                operators[i].updatePhaseInc(getSampleRate(), currentFreq);
                float fmModulation = 0.0f;
                for (int j = 0; j < numOps; ++j) {
                    fmModulation += opOutputs[j] * (fmMatrix[j][i] / 100.0f);
                }
                opOutputs[i] = operators[i].process(fmModulation, mixedSample);
                mixedSample += opOutputs[i]; 
            }
            
            for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel) {
                outputBuffer.addSample(channel, startSample + sample, mixedSample * level * 0.25f);
            }
        }
    }

    std::array<Operator, numOps> operators;
    float fmMatrix[numOps][numOps]{ {0.0f} };
private:
    float currentFreq = 0.0f;
    float level = 0.0f;
};
