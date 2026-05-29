// FMVoice.cpp
#include "FMVoice.h"

FMVoice::FMVoice()
{
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        // Pre-allocate base filters to avoid audio thread allocations later
        opFilters[i] = std::make_unique<SynthFilter>(); 
        
        for (int j = 0; j < ProjectConfig::numOperators; ++j)
        {
            matrixParams[i][j] = nullptr;
            audioMatrixParams[i][j] = nullptr;
            for (int row = 0; row < 3; ++row)
            {
                customModMatrixParams[row][i][j] = nullptr;
            }
        }
    }
}

bool FMVoice::canPlaySound (juce::SynthesiserSound* sound) 
{ 
    return true; 
}

void FMVoice::initParameters (juce::AudioProcessorValueTreeState& apvts)
{
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        juce::String opNum = juce::String (i + 1);

        opParams[i].mode       = apvts.getRawParameterValue ("MODE_" + opNum); 
        opParams[i].wave       = apvts.getRawParameterValue ("WAVE_" + opNum);
        opParams[i].filterType = apvts.getRawParameterValue ("FILTER_TYPE_" + opNum);
        opParams[i].ratio      = apvts.getRawParameterValue ("RATIO_" + opNum);
        opParams[i].detune     = apvts.getRawParameterValue ("DETUNE_" + opNum);
        opParams[i].phase      = apvts.getRawParameterValue ("PHASE_" + opNum);
        opParams[i].out        = apvts.getRawParameterValue ("OUT_" + opNum);
        opParams[i].attack     = apvts.getRawParameterValue ("ATTACK_" + opNum);
        opParams[i].decay      = apvts.getRawParameterValue ("DECAY_" + opNum);
        opParams[i].sustain    = apvts.getRawParameterValue ("SUSTAIN_" + opNum);
        opParams[i].release    = apvts.getRawParameterValue ("RELEASE_" + opNum);
        opParams[i].q          = apvts.getRawParameterValue ("FILTER_Q_" + opNum);
        opParams[i].sync       = apvts.getRawParameterValue ("TEMPO_SYNC_" + opNum);
	modSrcParams[i] = apvts.getRawParameterValue ("MOD_SRC_" + opNum);
        modTgtParams[i] = apvts.getRawParameterValue ("MOD_TGT_" + opNum);
        modAmtParams[i] = apvts.getRawParameterValue ("MOD_AMT_" + opNum);

        for (int dest = 0; dest < ProjectConfig::numOperators; ++dest)
        {
            matrixParams[i][dest] = apvts.getRawParameterValue ("MOD_" + juce::String (i) + "_" + juce::String (dest));
            audioMatrixParams[i][dest] = apvts.getRawParameterValue ("AUDIO_ROUTE_" + juce::String (i) + "_" + juce::String (dest));

            for (int row = 0; row < 3; ++row)
            {
                juce::String paramID = "FOCUSED_MOD_R" + juce::String(row + 1) + "_" + juce::String(i) + "_" + juce::String(dest);
                customModMatrixParams[row][i][dest] = apvts.getRawParameterValue (paramID);
            }
        }
    }
}

void FMVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    double safeRate = newRate > 0.0 ? newRate : 44100.0;
    
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        operators[i].prepare (safeRate);
        if (opFilters[i] != nullptr)
        {
            opFilters[i]->prepare (safeRate);
            opFilters[i]->reset();
        }
    }
}

void FMVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    baseFrequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    level = velocity;
    
    lastOpOutputs.fill (0.0f);
    previousOpOutputs.fill (0.0f);
    processedOpOutputs.fill (0.0f);

    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        if (opParams[i].attack != nullptr)
        {
            float initPhase = opParams[i].phase->load (std::memory_order_relaxed);
            operators[i].resetPhase (initPhase);
            juce::ADSR::Parameters p;
            p.attack  = opParams[i].attack->load (std::memory_order_relaxed);
            p.decay   = opParams[i].decay->load (std::memory_order_relaxed);
            p.sustain = opParams[i].sustain->load (std::memory_order_relaxed);
            p.release = opParams[i].release->load (std::memory_order_relaxed);
            
            operators[i].noteOn (p);
        }
        if (opFilters[i] != nullptr) opFilters[i]->reset();
    }
}

void FMVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff) { for (auto& op : operators) op.noteOff(); }
    else { clearCurrentNote(); }
}

void FMVoice::pitchWheelMoved (int) {}
void FMVoice::controllerMoved (int, int) {}

void FMVoice::setDAWTempo (float newBPM) noexcept
{
    currentBPM.store (newBPM, std::memory_order_relaxed);
}

void FMVoice::resetVoiceState()
{
    lastOpOutputs.fill (0.0f);
    previousOpOutputs.fill (0.0f);
    processedOpOutputs.fill (0.0f);

    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        if (opFilters[i] != nullptr) opFilters[i]->reset(); 
        operators[i].resetPhase (0.0f); 
    }
}

void FMVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!isVoiceActive() || opParams[0].ratio == nullptr) return;

    bool isAnyEnvelopeActive = false;
    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        if (operators[i].isActive()) 
        {
            isAnyEnvelopeActive = true;
            break;
        }
    }

    if (!isAnyEnvelopeActive)
    {
        lastOpOutputs.fill (0.0f);
        previousOpOutputs.fill (0.0f);
        processedOpOutputs.fill (0.0f);
    }

    double safeSampleRate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    std::array<float, ProjectConfig::numOperators> precalculatedK;

    for (int i = 0; i < ProjectConfig::numOperators; ++i)
    {
        float detune = opParams[i].detune->load (std::memory_order_relaxed);
        float resonanceQ = 0.1f + (detune * 4.9f);
        precalculatedK[i] = 1.0f / resonanceQ;
        
        int filterTypeChoice = static_cast<int>(opParams[i].filterType->load (std::memory_order_relaxed));
        if (opFilters[i] != nullptr) opFilters[i]->setType (filterTypeChoice);
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        std::array<float, ProjectConfig::numOperators> opOutputs { 0.0f };
        
        std::array<float, ProjectConfig::numOperators> pitchModOffsets { 0.0f };
        std::array<float, ProjectConfig::numOperators> phaseModOffsets { 0.0f };
        std::array<float, ProjectConfig::numOperators> levelModOffsets { 0.0f };
        std::array<float, ProjectConfig::numOperators> cutoffModOffsets { 0.0f };
        std::array<float, ProjectConfig::numOperators> qModOffsets { 0.0f };

        // Matrix Modulations
        for (int row = 0; row < 3; ++row)
        {
            if (rowTargetParams[row] == nullptr) continue;
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
                float modIndex = matrixParams[src][dest]->load (std::memory_order_relaxed);
                if (modIndex > 0.0f)
                {
                    float modSignal = (src == dest) ? (lastOpOutputs[src] + previousOpOutputs[src]) * 0.5f : lastOpOutputs[src];
                    modulationSum += modSignal * modIndex;
                }
    
                float audioGain = audioMatrixParams[src][dest]->load (std::memory_order_relaxed);
                if (audioGain > 0.0f)
                {
                    audioInputSum += lastOpOutputs[src] * audioGain;
                }
            }
            audioInputSum = std::tanh (audioInputSum);
            
            float ratio = opParams[dest].ratio->load (std::memory_order_relaxed);
            int opMode = static_cast<int> (opParams[dest].mode->load (std::memory_order_relaxed));
            int opShape = static_cast<int> (opParams[dest].wave->load (std::memory_order_relaxed));
            
            int currentFilterType = (opFilters[dest] != nullptr) ? opFilters[dest]->getCurrentType() : 0;
            
            float currentQ = 0.707f;
            if (opParams[dest].q != nullptr) { currentQ = opParams[dest].q->load (std::memory_order_relaxed); }
            currentQ = juce::jlimit (0.05f, 25.0f, currentQ + (qModOffsets[dest] * 5.0f));
            
            if (opMode == 2 && opFilters[dest] != nullptr) 
            {
                if (currentFilterType == 3) 
                {
                    float feedbackKnob = opParams[dest].attack->load (std::memory_order_relaxed);
                    float dampingKnob  = opParams[dest].decay->load (std::memory_order_relaxed);
                    float freqKnob     = opParams[dest].sustain->load (std::memory_order_relaxed);
                    float keytrack     = opParams[dest].release->load (std::memory_order_relaxed);

                    float feedbackAmt = feedbackKnob * 0.99f;
                    float dampingAmt  = dampingKnob * 0.95f;
                    float combFreq = 1000.0f;

                    if (keytrack > 0.5f)
                    {
                        float ratioMult = opParams[dest].ratio->load (std::memory_order_relaxed);
                        float octaveShift = (freqKnob * 4.0f) - 2.0f;
                        combFreq = baseFrequency * ratioMult * std::pow (2.0f, octaveShift);
                    }
                    else
                    {
                        combFreq = 20.0f * std::pow (500.0f, freqKnob);
                    }

                    combFreq += (modulationSum * 200.0f) + (cutoffModOffsets[dest] * 4000.0f);
                    combFreq = juce::jlimit (20.0f, static_cast<float>(safeSampleRate) * 0.49f, combFreq);

                    opOutputs[dest] = opFilters[dest]->processSampleComb (audioInputSum, combFreq, feedbackAmt, dampingAmt);
                }
                else 
                {
                    float ratioKnob = opParams[dest].ratio->load (std::memory_order_relaxed);
                    float normalizedKnob = (ratioKnob - 0.25f) / (16.0f - 0.25f);
                    float baseCutoff = 20.0f * std::pow (1000.0f, normalizedKnob);
                    
                    modulationSum = std::tanh (modulationSum * 0.2f) * 5.0f;
                    
                    opFilters[dest]->setResonance (currentQ);
                    float currentK = opFilters[dest]->getPrecalculatedK();
                    float dynamicCutoff = baseCutoff + (modulationSum * 5000.0f) + (cutoffModOffsets[dest] * 4000.0f);
                    dynamicCutoff = juce::jlimit (20.0f, static_cast<float>(safeSampleRate) * 0.49f, dynamicCutoff);
                    
                    opOutputs[dest] = opFilters[dest]->processSampleAudioRate (audioInputSum, dynamicCutoff, currentK);
                }
            }
            else 
            {
                float nodeTargetFrequency = baseFrequency;
                if (opParams[dest].sync != nullptr && opParams[dest].sync->load(std::memory_order_relaxed) > 0.5f)
                {
                    float bpm = currentBPM.load (std::memory_order_relaxed);
                    nodeTargetFrequency = bpm / 60.0f;
                }
                
                float semitoneOffset = pitchModOffsets[dest] * 12.0f;
                float modulatedFreq = nodeTargetFrequency * std::pow (2.0f, semitoneOffset / 12.0f);
                modulatedFreq = juce::jlimit(0.1f, static_cast<float>(safeSampleRate) * 0.49f, modulatedFreq);
                
                modulationSum = std::tanh (modulationSum * 0.15f) * (juce::MathConstants<float>::pi * 2.0f);
                    
                float phaseOffset = opParams[dest].phase->load (std::memory_order_relaxed) + (phaseModOffsets[dest] * 360.0f);
                float detune = opParams[dest].detune->load (std::memory_order_relaxed);
                        
                opOutputs[dest] = operators[dest].processSample (
                    modulatedFreq, ratio, detune, modulationSum,
                    audioInputSum, opMode, opShape, currentFilterType, currentQ
                );
                        
                opOutputs[dest] = std::tanh (opOutputs[dest] + audioInputSum);
            }
                    
            processedOpOutputs[dest] = opOutputs[dest];
                    
            if (std::abs(levelModOffsets[dest]) > 0.001f)
            {
                processedOpOutputs[dest] *= juce::jlimit(0.0f, 2.0f, 1.0f + levelModOffsets[dest]);
            }
        }
        
        previousOpOutputs = lastOpOutputs;
        lastOpOutputs = processedOpOutputs; 

        float mixSample = 0.0f;
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
        {
            float outLevel = opParams[i].out->load (std::memory_order_relaxed);
            mixSample += processedOpOutputs[i] * outLevel; 
        }

        float polyphonyCushion = 1.0f / std::sqrt (static_cast<float> (ProjectConfig::numOperators));
        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
        {
            float finalGain = level * polyphonyCushion * 0.5f;
            outputBuffer.addSample (channel, startSample + sample, mixSample * finalGain);
        }
    }
}
