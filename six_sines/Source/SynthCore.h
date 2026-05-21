#pragma once
#include <JuceHeader.h>

struct LFOParams {
    float freq = 1.0f;
    float amount = 0.0f;
    int wave = 0; // 0: Sine, 1: Tri, 2: Saw, 3: Sqr
    bool sync = true;
};

struct VoiceParams {
    float adsr1[4], adsr2[4];
    int opAdsrSource[6]; 
    bool envRetrigger[2];
    int modes[6];
    float ratios[6], gains[6], gainsOut[6], res[6], res2[6], kbTrack[6], matrix[6][6];
    LFOParams lfos[2];
    float modWheelValue = 0.0f;
    int modSource[8], modDest[8];
    float modAmount[8];
    int lfoDirectDest[2];
    float lfoDirectDepth[2];
};

class Operator {
public:
    void prepare(double sr, int samplesPerBlock) {
        sampleRate = sr;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32)samplesPerBlock, 1 };
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        delayLine.prepare(spec);
        delayLine.setMaximumDelayInSamples(sr * 0.5); 
        reset();
    }
    void reset() { phase = 0.0f; filter.reset(); delayLine.reset(); }

    float process(float input, float hz, float pMod, float c1, float c2, float keyTrack, float noteHz, int modeIdx) {
        if (modeIdx == 0) { // Sine
            float out = std::sin(phase + pMod + (c1 * juce::MathConstants<float>::twoPi));
            phase += (juce::MathConstants<float>::twoPi * hz) / (float)sampleRate;
            if (phase >= juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;
            return out + input;
        } else if (modeIdx == 1) { // LoPass
            float finalCutoff = juce::jlimit(20.0f, 20000.0f, juce::jmap(c1, 20.0f, 20000.0f) + (noteHz - 261.63f) * keyTrack);
            filter.setCutoffFrequency(finalCutoff);
            filter.setResonance(juce::jlimit(0.1f, 10.0f, (c2 * 9.9f) + 0.1f));
            return filter.processSample(0, input);
        } else { // Comb
            delayLine.setDelay((float)sampleRate / juce::jlimit(20.0f, 18000.0f, juce::jmap(c2, 20.0f, 10000.0f)));
            float delayed = delayLine.popSample(0);
            delayLine.pushSample(0, input + (delayed * juce::jlimit(0.0f, 0.98f, c1)));
            return input + delayed;
        }
    }
private:
    double sampleRate = 44100.0; float phase = 0.0f;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::DelayLine<float> delayLine { 50000 };
};

struct FMSound : public juce::SynthesiserSound {
    FMSound() {}
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class FMVoice : public juce::SynthesiserVoice {
public:
    FMVoice() { for (int i = 0; i < 6; ++i) ops[i] = std::make_unique<Operator>(); }
    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<FMSound*>(s) != nullptr; }
    void prepare(double sr, int samplesPerBlock) { adsr1.setSampleRate(sr); adsr2.setSampleRate(sr); for (auto& o : ops) o->prepare(sr, samplesPerBlock); }
    void setCurrentParams(const VoiceParams& p) { params = p; }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override {
        noteHz = (float)juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        juce::ADSR::Parameters p1, p2;
        p1.attack = params.adsr1[0]; p1.decay = params.adsr1[1]; p1.sustain = params.adsr1[2]; p1.release = params.adsr1[3];
        p2.attack = params.adsr2[0]; p2.decay = params.adsr2[1]; p2.sustain = params.adsr2[2]; p2.release = params.adsr2[3];
        adsr1.setParameters(p1); adsr2.setParameters(p2);
        if (params.envRetrigger[0]) adsr1.reset();
        if (params.envRetrigger[1]) adsr2.reset();
        for (int i=0; i<2; ++i) if(params.lfos[i].sync) lfoPhases[i] = 0.0f;
        for (auto& o : ops) o->reset();
        adsr1.noteOn(); adsr2.noteOn();
    }

    void stopNote (float, bool allowTailOff) override { adsr1.noteOff(); adsr2.noteOff(); if (!allowTailOff) clearCurrentNote(); }

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override {
        for (int s = 0; s < numSamples; ++s) {
            float lfoVals[2] = {0.0f, 0.0f};
            for(int k=0; k<2; ++k) {
                float raw = std::sin(lfoPhases[k]);
                if(params.lfos[k].wave == 1) raw = (std::asin(raw) * 0.6366f);
                else if(params.lfos[k].wave == 2) raw = (lfoPhases[k] / juce::MathConstants<float>::pi) - 1.0f;
                else if(params.lfos[k].wave == 3) raw = (lfoPhases[k] < juce::MathConstants<float>::pi) ? 1.0f : -1.0f;
                lfoVals[k] = raw * params.lfos[k].amount;
                lfoPhases[k] += (juce::MathConstants<float>::twoPi * params.lfos[k].freq) / getSampleRate();
                if(lfoPhases[k] >= juce::MathConstants<float>::twoPi) lfoPhases[k] -= juce::MathConstants<float>::twoPi;
            }

            float modOffsets[55] = {0.0f};
            for (int i=0; i<8; ++i) {
                if (params.modDest[i] == 0) continue;
                float src = (params.modSource[i] == 0) ? lfoVals[0] : (params.modSource[i] == 1 ? lfoVals[1] : params.modWheelValue);
                modOffsets[params.modDest[i]] += (src * params.modAmount[i]);
            }
            for (int i=0; i<2; ++i) if (params.lfoDirectDest[i] > 0) modOffsets[params.lfoDirectDest[i]] += (lfoVals[i] * params.lfoDirectDepth[i]);

            float env1 = adsr1.getNextSample(), env2 = adsr2.getNextSample();
            float chain = 0.0f, sum = 0.0f, opOuts[6] = {0.0f};

            for (int i = 0; i < 6; ++i) {
                float pMod = 0.0f;
                for (int j = 0; j < 6; ++j) pMod += opOuts[j] * juce::jlimit(-1.0f, 1.0f, params.matrix[i][j] + modOffsets[19 + (i*6) + j]);
                float curEnv = (params.opAdsrSource[i] == 0) ? env1 : env2;
                float curG = juce::jlimit(0.0f, 1.0f, params.gains[i] + modOffsets[i+1]);
                float curR = juce::jlimit(0.1f, 20.0f, params.ratios[i] + modOffsets[i+7]);
                float curC1 = juce::jlimit(0.0f, 1.0f, params.res[i] + modOffsets[i+13]);
                float out = ops[i]->process(chain, noteHz * curR, pMod, curC1, params.res2[i], params.kbTrack[i], noteHz, params.modes[i]);
                sum += out * params.gainsOut[i] * curEnv;
                chain = out * curG * curEnv;
                opOuts[i] = out;
            }
            float final = (sum + chain) * 0.2f;
            for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch) outputBuffer.addSample(ch, startSample + s, final);
            if (!adsr1.isActive() && !adsr2.isActive()) clearCurrentNote();
        }
    }
    void pitchWheelMoved (int) override {}
    void controllerMoved (int num, int val) override { if (num == 1) params.modWheelValue = (float)val / 127.0f; }
private:
    std::unique_ptr<Operator> ops[6]; juce::ADSR adsr1, adsr2; VoiceParams params; float noteHz = 0.0f, lfoPhases[2] = {0.0f, 0.0f};
};
