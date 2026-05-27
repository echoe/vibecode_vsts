// FMVoice.h
#pragma once
#include <JuceHeader.h>
#include "FMOperator.h"
#include "SynthFilter.h"
#include "Constants.h"

class FMVoice : public juce::SynthesiserVoice
{
public:
    FMVoice()
    {
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            waveParams[i] = nullptr;
            ratioParams[i] = nullptr;
            detuneParams[i] = nullptr;
            phaseParams[i] = nullptr;
            outParams[i] = nullptr;
            attackParams[i] = nullptr;
            decayParams[i] = nullptr;
            sustainParams[i] = nullptr;
            releaseParams[i] = nullptr;
            syncParams[i] = nullptr;
            qParams[i] = nullptr;

            for (int j = 0; j < ProjectConfig::numOperators; ++j)
            {
                matrixParams[i][j] = nullptr;
                audioMatrixParams[i][j] = nullptr;
                
                // Initialize new custom modulation matrices for the 3 visual matrix rows
                for (int row = 0; row < 3; ++row)
                {
                    customModMatrixParams[row][i][j] = nullptr;
                }
            }
        }
    }

    bool canPlaySound (juce::SynthesiserSound* sound) override { return true; }

    void initParameters (juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            juce::String opNum = juce::String (i + 1);

            waveParams[i] = apvts.getRawParameterValue ("WAVE_" + opNum);
            filterTypeParams[i] = apvts.getRawParameterValue ("FILTER_TYPE_" + opNum);
            ratioParams[i]   = apvts.getRawParameterValue ("RATIO_" + opNum);
            detuneParams[i]  = apvts.getRawParameterValue ("DETUNE_" + opNum);
            phaseParams[i] = apvts.getRawParameterValue ("PHASE_" + opNum);
            outParams[i]     = apvts.getRawParameterValue ("OUT_" + opNum);
            attackParams[i]  = apvts.getRawParameterValue ("ATTACK_" + opNum);
            decayParams[i]   = apvts.getRawParameterValue ("DECAY_" + opNum);
            sustainParams[i] = apvts.getRawParameterValue ("SUSTAIN_" + opNum);
            releaseParams[i] = apvts.getRawParameterValue ("RELEASE_" + opNum);
            qParams[i] = apvts.getRawParameterValue ("FILTER_Q_" + opNum);
            syncParams[i] = apvts.getRawParameterValue ("TEMPO_SYNC_" + opNum);

            // Row-level target tracking parameters (store integers matching target enum selections)
            rowTargetParams[0] = apvts.getRawParameterValue ("ROW_TARGET_1");
            rowTargetParams[1] = apvts.getRawParameterValue ("ROW_TARGET_2");
            rowTargetParams[2] = apvts.getRawParameterValue ("ROW_TARGET_3");

            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
            {
                juce::String matrixID = "MOD_" + juce::String (i) + "_" + juce::String (dest);
                matrixParams[i][dest] = apvts.getRawParameterValue (matrixID);
                
                juce::String audioMatrixID = "AUDIO_ROUTE_" + juce::String (i) + "_" + juce::String (dest);
                audioMatrixParams[i][dest] = apvts.getRawParameterValue (audioMatrixID);

                // Cache every underlying background node in the massive matrix grid safely
                for (int row = 0; row < 3; ++row)
                {
                    juce::String paramID = "FOCUSED_MOD_R" + juce::String(row + 1) + "_" + juce::String(i) + "_" + juce::String(dest);
                    customModMatrixParams[row][i][dest] = apvts.getRawParameterValue (paramID);
                }
            }
        }
    }

    void setCurrentPlaybackSampleRate (double newRate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
        double safeRate = newRate > 0.0 ? newRate : 44100.0;
    
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            operators[i].prepare (safeRate); 
            opFilters[i].prepare (safeRate); 
            opFilters[i].reset();
        }   
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        baseFrequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        level = velocity;
        // clear history of audio so we don't get stuff carried over from voice to voice causing bad sounds
        lastOpOutputs.fill (0.0f);
        previousOpOutputs.fill (0.0f);
        processedOpOutputs.fill (0.0f);

        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            if (attackParams[i] != nullptr)
            {
                float initPhase = phaseParams[i]->load (std::memory_order_relaxed);
                operators[i].resetPhase (initPhase);
                juce::ADSR::Parameters p;
                p.attack  = attackParams[i]->load (std::memory_order_relaxed);
                p.decay   = decayParams[i]->load (std::memory_order_relaxed);
                p.sustain = sustainParams[i]->load (std::memory_order_relaxed);
                p.release = releaseParams[i]->load (std::memory_order_relaxed);
                
                operators[i].noteOn (p);
            }
            opFilters[i].reset(); 
        }
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) { for (auto& op : operators) op.noteOff(); }
        else { clearCurrentNote(); }
    }

    void pitchWheelMoved (int newPitchWheelValue) override {}
    void controllerMoved (int controllerNumber, int newControllerValue) override {}

    void setDAWTempo (float newBPM) noexcept
    {
        currentBPM.store (newBPM, std::memory_order_relaxed);
    }
    
    void resetVoiceState()
    {
        // 1. Clear sample-to-sample feedback arrays completely
        lastOpOutputs.fill (0.0f);
        previousOpOutputs.fill (0.0f);
        processedOpOutputs.fill (0.0f);

        // 2. Clear the historical state registers of every internal operator filter
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            opFilters[i].reset(); // Purges internal biquad history registers
            operators[i].resetPhase (0.0f); // Resets the oscillator lookup indexes
        }
    }

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (ratioParams[0] == nullptr) return;
        // let's make sure we don't act up around loops
        bool isAnyEnvelopeActive = false;
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            if (operators[i].isActive()) // Check if the underlying ADSR is still ticking
            {
                isAnyEnvelopeActive = true;
                break;
            }
        }

        if (!isAnyEnvelopeActive)
        {
            // If no notes are sustaining, historical arrays MUST be pure silence.
            lastOpOutputs.fill (0.0f);
            previousOpOutputs.fill (0.0f);
            processedOpOutputs.fill (0.0f);
        }
    
        double safeSampleRate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    
        // Precalculate resonance 'k' at block-rate
        std::array<float, ProjectConfig::numOperators> precalculatedK;
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            float detune = detuneParams[i]->load (std::memory_order_relaxed);
            float resonanceQ = 0.1f + (detune * 4.9f);
            precalculatedK[i] = 1.0f / resonanceQ;
            
            int filterTypeChoice = static_cast<int>(filterTypeParams[i]->load (std::memory_order_relaxed));
            opFilters[i].setType (filterTypeChoice);
        }
    
        // Main Sample Loop
        for (int sample = 0; sample < numSamples; ++sample)
        {
            std::array<float, ProjectConfig::numOperators> opOutputs { 0.0f };
            
            // Per-sample arrays to capture cumulative variable modulation adjustments
            std::array<float, ProjectConfig::numOperators> pitchModOffsets { 0.0f };
            std::array<float, ProjectConfig::numOperators> phaseModOffsets { 0.0f };
            std::array<float, ProjectConfig::numOperators> levelModOffsets { 0.0f };
            std::array<float, ProjectConfig::numOperators> cutoffModOffsets { 0.0f };
            std::array<float, ProjectConfig::numOperators> qModOffsets { 0.0f };

            // --- EVALUATE MULTI-ROW MATRIX MODULATIONS ---
            for (int row = 0; row < 3; ++row)
            {
                if (rowTargetParams[row] == nullptr) continue;
                
                // Read what parameter this row is targeting: 0=Pitch, 1=Phase, 2=Level, 3=Cutoff, 4=Resonance
                int targetType = static_cast<int>(rowTargetParams[row]->load (std::memory_order_relaxed));

                for (int src = 0; src < ProjectConfig::numOperators; ++src)
                {
                    float srcAudio = lastOpOutputs[src];

                    for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
                    {
                        if (customModMatrixParams[row][src][dest] == nullptr) continue;
                        
                        float amt = customModMatrixParams[row][src][dest]->load (std::memory_order_relaxed);
                        if (std::abs(amt) < 0.0001f) continue;

                        float modSignal = srcAudio * amt;

                        if (targetType == 0)      pitchModOffsets[dest]  += modSignal;
                        else if (targetType == 1) phaseModOffsets[dest]  += modSignal;
                        else if (targetType == 2) levelModOffsets[dest]  += modSignal;
                        else if (targetType == 3) cutoffModOffsets[dest] += modSignal;
                        else if (targetType == 4) qModOffsets[dest]      += modSignal;
                    }
                }
            }
        
            for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
                {
                    float modulationSum = 0.0f;
                    float audioInputSum = 0.0f;
            
                    for (int src = 0; src < ProjectConfig::numOperators; ++src)
                    {
                        // --- GRID 1: FM PHASE MODULATION MATRIX ---
                        float modIndex = matrixParams[src][dest]->load (std::memory_order_relaxed);
                        if (modIndex > 0.0f)
                        {
                            float modSignal = (src == dest) ? (lastOpOutputs[src] + previousOpOutputs[src]) * 0.5f : lastOpOutputs[src];
                            // this makes it safe and react more like the DX7
                            float scaledIndex = modIndex * juce::MathConstants<float>::pi;
                            modulationSum += modSignal * modIndex;
                        }
            
                        // --- GRID 2: RAW AUDIO ROUTING MATRIX ---
                        float audioGain = audioMatrixParams[src][dest]->load (std::memory_order_relaxed);
                        if (audioGain > 0.0f)
                        {
                            audioInputSum += lastOpOutputs[src] * audioGain;
                        }
                    }
                    // GAIN STAGING CUSHION
                    audioInputSum = std::tanh (audioInputSum);
                    // =================================================================
                    
                    float ratio = ratioParams[dest]->load (std::memory_order_relaxed);
                    int waveType = static_cast<int> (waveParams[dest]->load (std::memory_order_relaxed));
                    
                    if (waveType == 5) // If this operator is acting as a filter node
                    {
                        float ratioKnob = ratioParams[dest]->load (std::memory_order_relaxed);
                        float normalizedKnob = (ratioKnob - 0.25f) / (16.0f - 0.25f);
                        float baseCutoff = 20.0f * std::pow (1000.0f, normalizedKnob);
                        
                        modulationSum = std::tanh (modulationSum * 0.2f) * 5.0f;
                        
                        float currentQ = 0.707f;
                        if (qParams[dest] != nullptr) { currentQ = qParams[dest]->load (std::memory_order_relaxed); }
                                
                        // Apply Resonance Modulation (Safely clamp results to prevent biquad explosions)
                        currentQ += (qModOffsets[dest] * 5.0f);
                        currentQ = juce::jlimit (0.05f, 25.0f, currentQ);
                                
                        opFilters[dest].setResonance (currentQ);
                        float currentK = opFilters[dest].getPrecalculatedK();
                                
                        float scaledDepthHz = 5000.0f;
                        
                        // Apply Cutoff Modulation (Adds directly to the physical mapping path)
                        float dynamicCutoff = baseCutoff + (modulationSum * scaledDepthHz) + (cutoffModOffsets[dest] * 4000.0f);
                            
                        float maxCutoff = static_cast<float>(safeSampleRate) * 0.49f;
                        dynamicCutoff = juce::jlimit (20.0f, maxCutoff, dynamicCutoff);
                            
                        opOutputs[dest] = opFilters[dest].processSampleAudioRate (audioInputSum, dynamicCutoff, currentK);
                    }
                    else // DESTINATION NODE IS A STANDARD OSCILLATOR
                    {
                        float nodeTargetFrequency = baseFrequency;
                        if (syncParams[dest] != nullptr && syncParams[dest]->load(std::memory_order_relaxed) > 0.5f)
                        {
                            float bpm = currentBPM.load (std::memory_order_relaxed);
                            nodeTargetFrequency = bpm / 60.0f;
                        }
                        // EXPONENTIAL PITCH MODULATION (Protects against internal phase clipping)
                        // Multiplying by 12.0f means the matrix can modulate pitch by +/- 12 semitones (1 Octave).
                        // =================================================================
                        float semitoneOffset = pitchModOffsets[dest] * 12.0f;
                        float modulatedFreq = nodeTargetFrequency * std::pow (2.0f, semitoneOffset / 12.0f);
                        modulatedFreq = juce::jlimit(0.1f, static_cast<float>(safeSampleRate) * 0.49f, modulatedFreq);
                        // =================================================================
                        // Stabilize the output path phasesum - testing. normal is below
                        modulationSum = std::tanh (modulationSum * 0.15f) * (juce::MathConstants<float>::pi * 2.0f);
                        //modulationSum = std::tanh (modulationSum * 0.2f) * 5.0f;
                                
                        // Apply Phase Offset Modulation
                        float phaseOffset = phaseParams[dest]->load (std::memory_order_relaxed) + (phaseModOffsets[dest] * 360.0f);
                        float detune = detuneParams[dest]->load (std::memory_order_relaxed);
                                
                        opOutputs[dest] = operators[dest].processSample (modulatedFreq, ratio, detune, modulationSum, waveType);
                                
                        // CRITICAL STABILITY: Soft-clip the sum here too so adding the node audio
                        // doesn't clip the operator output tracking step immediately below.
                        opOutputs[dest] = std::tanh (opOutputs[dest] + audioInputSum);
                    }
                            
                    // Store structural modifications inside an internal matrix processing cache layer
                    processedOpOutputs[dest] = opOutputs[dest];
                            
                    // Apply dynamic Amplitude/Level modulation to the audio array
                    if (std::abs(levelModOffsets[dest]) > 0.001f)
                    {
                        processedOpOutputs[dest] *= juce::jlimit(0.0f, 2.0f, 1.0f + levelModOffsets[dest]);
                    }
                }
            previousOpOutputs = lastOpOutputs;
            // lastOpOutputs = opOutputs; // this would be if you want the raw output and not the processed stuff tracking update
            lastOpOutputs = processedOpOutputs; // Use finalized, gain-staged outputs for history tracking!

            float mixSample = 0.0f;
            for (int i = 0; i < ProjectConfig::numOperators; ++i)
            {
                float outLevel = outParams[i]->load (std::memory_order_relaxed);
                mixSample += processedOpOutputs[i] * outLevel; // Use the modulated signals for the master output
            }
            // --- MASTER GAIN COMPENSATION FOR POLYPHONY ---
            // A cushion of 1.0 / sqrt(numVoices) is the standard musical way to keep
            // volume stable without making single notes sound too quiet.
            float polyphonyCushion = 1.0f / std::sqrt (static_cast<float> (ProjectConfig::numOperators));
            for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            {
                // Combine level, your safety cushion, and a reasonable master scalar
                float finalGain = level * polyphonyCushion * 0.5f;
                outputBuffer.addSample (channel, startSample + sample, mixSample * finalGain);
            }
        }

        bool anyOscillatorActive = false;
        for (auto& op : operators) { if (op.isActive()) { anyOscillatorActive = true; break; } }
        if (!anyOscillatorActive){ clearCurrentNote();}
    }

private:
    double baseFrequency = 440.0;
    float level = 0.0f;
    std::atomic<float> currentBPM { 120.0f }; 

    std::array<FMOperator, ProjectConfig::numOperators> operators;
    std::atomic<float>* filterTypeParams[ProjectConfig::numOperators];
    SynthFilter opFilters[ProjectConfig::numOperators];
    std::array<float, ProjectConfig::numOperators> lastOpOutputs { 0.0f };
    std::array<float, ProjectConfig::numOperators> previousOpOutputs { 0.0f };
    std::array<float, ProjectConfig::numOperators> processedOpOutputs { 0.0f }; // Array for level modulation caching

    std::array<std::atomic<float>*, ProjectConfig::numOperators> waveParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> ratioParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> detuneParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> phaseParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> outParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> attackParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> decayParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> sustainParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> releaseParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> syncParams;
    std::array<std::atomic<float>*, ProjectConfig::numOperators> qParams;
    
    // Tracks current parameter assignment for each row (0=Pitch, 1=Phase, etc.)
    std::array<std::atomic<float>*, 3> rowTargetParams;

    std::array<std::array<std::atomic<float>*, ProjectConfig::numOperators>, ProjectConfig::numOperators> matrixParams;
    std::array<std::array<std::atomic<float>*, ProjectConfig::numOperators>, ProjectConfig::numOperators> audioMatrixParams;
    
    // Massive background storage grid: 3 rows, 6 sources, 6 destinations
    std::array<std::array<std::array<std::atomic<float>*, ProjectConfig::numOperators>, ProjectConfig::numOperators>, 3> customModMatrixParams;
};
