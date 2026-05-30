#pragma once
#include <JuceHeader.h>
#include "FMOperator.h"
#include "SynthFilter.h"
#include "Constants.h"
#include <array>
#include <memory>

struct OperatorParameterCache 
{
    std::atomic<float>* mode { nullptr };
    std::atomic<float>* wave { nullptr };
    std::atomic<float>* filterType { nullptr };
    std::atomic<float>* ratio { nullptr };
    std::atomic<float>* detune { nullptr };
    std::atomic<float>* phase { nullptr };
    std::atomic<float>* fold { nullptr };
    std::atomic<float>* out { nullptr };
    std::atomic<float>* attack { nullptr };
    std::atomic<float>* decay { nullptr };
    std::atomic<float>* sustain { nullptr };
    std::atomic<float>* release { nullptr };
    std::atomic<float>* sync { nullptr };
    std::atomic<float>* q { nullptr };
};

class FMVoice : public juce::SynthesiserVoice
{
public:
    FMVoice();
    ~FMVoice() override = default;

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void initParameters (juce::AudioProcessorValueTreeState& apvts);
    void setCurrentPlaybackSampleRate (double newRate) override;
    
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int controllerNumber, int newControllerValue) override;
    
    void setDAWTempo (float newBPM) noexcept;
    void resetVoiceState();
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    double baseFrequency { 440.0 };
    float level { 0.0f };
    int lastPlayedNote = 60; 
    std::atomic<float> currentBPM { 120.0f };

    std::array<FMOperator, ProjectConfig::numOperators> operators;

    std::array<float, ProjectConfig::numOperators> lastOpOutputs { 0.0f };
    std::array<float, ProjectConfig::numOperators> previousOpOutputs { 0.0f };
    std::array<float, ProjectConfig::numOperators> processedOpOutputs { 0.0f };

    std::array<OperatorParameterCache, ProjectConfig::numOperators> opParams;
    std::array<std::atomic<float>*, 6> extraModParams { nullptr };

    std::array<std::array<std::atomic<float>*, ProjectConfig::numOperators>, ProjectConfig::numOperators> matrixParams {};
    std::array<std::array<std::atomic<float>*, ProjectConfig::numOperators>, ProjectConfig::numOperators> audioMatrixParams {};
    std::array<std::array<std::array<std::atomic<float>*, ProjectConfig::numOperators>, ProjectConfig::numOperators>, 3> customModMatrixParams {};

    std::array<std::atomic<float>*, 6> modSrcParams { nullptr };
    std::array<std::atomic<float>*, 6> modTgtParams { nullptr };
    std::array<std::atomic<float>*, 6> modAmtParams { nullptr };
};
