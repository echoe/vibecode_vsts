#include "PluginProcessor.h"
#include <cmath>

SubharmonicSequencerAudioProcessor::SubharmonicSequencerAudioProcessor()
    : AudioProcessor (BusesProperties()),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

SubharmonicSequencerAudioProcessor::~SubharmonicSequencerAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout SubharmonicSequencerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Global Sequence Parameters
    layout.add (std::make_unique<juce::AudioParameterInt> ("MAIN_LENGTH", "Main Steps Length", 1, 8, 4));
    layout.add (std::make_unique<juce::AudioParameterInt> ("SUB_LENGTH", "Sub Steps Length", 1, 8, 4));
    layout.add (std::make_unique<juce::AudioParameterInt> ("SUB_RATIO", "Subharmonic Ratio", 2, 8, 2));

    // Dynamic Generation of per-step parameters for Step 1 through 8
    for (int i = 1; i <= 8; ++i)
    {
        juce::String stepId = juce::String (i);
        
        // Main Step Parameters
        layout.add (std::make_unique<juce::AudioParameterInt> ("MAIN_NOTE_" + stepId, "Main Step " + stepId + " Note", 0, 127, 60));
        layout.add (std::make_unique<juce::AudioParameterFloat> ("MAIN_VEL_" + stepId, "Main Step " + stepId + " Vel", 0.0f, 1.0f, 0.8f));
        layout.add (std::make_unique<juce::AudioParameterFloat> ("MAIN_LEN_" + stepId, "Main Step " + stepId + " Length", 0.05f, 1.0f, 0.5f));

        // Subharmonic Step Parameters
        layout.add (std::make_unique<juce::AudioParameterBool> ("SUB_ACTIVE_" + stepId, "Sub Step " + stepId + " Active", true));
        layout.add (std::make_unique<juce::AudioParameterFloat> ("SUB_VEL_" + stepId, "Sub Step " + stepId + " Vel", 0.0f, 1.0f, 0.7f));
        layout.add (std::make_unique<juce::AudioParameterFloat> ("SUB_LEN_" + stepId, "Sub Step " + stepId + " Length", 0.05f, 1.0f, 0.5f));
    }

    return layout;
}

void SubharmonicSequencerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    lastPpqPosition = -1.0;
    activeNoteOffs.clear();
}

void SubharmonicSequencerAudioProcessor::releaseResources() {}

int SubharmonicSequencerAudioProcessor::calculateSubharmonicNote (int mainNote, int divisionFactor)
{
    if (divisionFactor <= 1) return mainNote;

    // Convert MIDI Note to exact Frequency (Hz)
    double mainFrequency = 440.0 * std::pow (2.0, (mainNote - 69) / 12.0);
    
    // Divide target frequency mathematically
    double subFrequency = mainFrequency / double (divisionFactor);

    // Safeguard sub-audio spectrum calculations
    if (subFrequency < 8.18) return 0; 

    // Convert Frequency back into nearest MIDI Note number
    int subNote = std::round (69.0 + 12.0 * std::log2 (subFrequency / 440.0));
    return juce::jlimit (0, 127, subNote);
}

void SubharmonicSequencerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // A pure MIDI effect should clear incoming audio signals passing through it
    buffer.clear();

    auto* playHead = getPlayHead();
    if (playHead == nullptr) return;

    auto positionInfo = playHead->getPosition();
    if (!positionInfo.hasValue() || !positionInfo->getIsPlaying())
    {
        // If the DAW stops, immediately flush and send note-offs for hanging notes
        if (!activeNoteOffs.empty())
        {
            for (const auto& noteOff : activeNoteOffs) {
                midiMessages.addEvent (juce::MidiMessage::noteOff (1, noteOff.midiNote), 0);
            }
            activeNoteOffs.clear();
        }
        lastPpqPosition = -1.0;
        return;
    }

    double bpm = positionInfo->getBpm().orFallback (120.0);
    double startPpq = positionInfo->getPpqPosition().orFallback (0.0);
    
    // Calculate where this buffer block ends inside the timeline (expressed in terms of PPQ)
    double numSamples = buffer.getNumSamples();
    double sampleRate = getSampleRate();
    double beatsPerSecond = bpm / 60.0;
    double bufferDurationInSeconds = numSamples / sampleRate;
    double endPpq = startPpq + (bufferDurationInSeconds * beatsPerSecond);

    // 1. First process any pending note-offs falling inside this time-slice window
    auto it = activeNoteOffs.begin();
    while (it != activeNoteOffs.end())
    {
        if (it->offPpqPosition >= startPpq && it->offPpqPosition < endPpq)
        {
            // Map the exact PPQ position to a sample offset inside the audio buffer
            double ratio = (it->offPpqPosition - startPpq) / (endPpq - startPpq);
            int sampleOffset = juce::jlimit (0, int (numSamples - 1), int (ratio * numSamples));
            
            midiMessages.addEvent (juce::MidiMessage::noteOff (1, it->midiNote), sampleOffset);
            it = activeNoteOffs.erase (it);
        }
        else
        {
            ++it;
        }
    }

    // 2. Check for sequence steps triggers
    handleMidiTriggers (startPpq, endPpq, bpm, midiMessages);

    lastPpqPosition = startPpq;
}

void SubharmonicSequencerAudioProcessor::handleMidiTriggers (double startPpq, double endPpq, double bpm, juce::MidiBuffer& midiMessages)
{
    // We assume standard 16th note steps (0.25 Pulses per Quarter Note per step)
    const double stepDurationPpq = 0.25;

    // Find the step boundaries encompassing our current buffer window
    int startStepIndex = std::floor (startPpq / stepDurationPpq);
    int endStepIndex   = std::floor (endPpq / stepDurationPpq);

    // If we have transitioned onto a brand new step boundary
    if (startStepIndex != std::floor (lastPpqPosition / stepDurationPpq) || lastPpqPosition < 0)
    {
        // Fetch current sequence limitations from our thread-safe parameters
        int mainMaxSteps = *apvts.getRawParameterValue ("MAIN_LENGTH");
        int subMaxSteps  = *apvts.getRawParameterValue ("SUB_LENGTH");
        int subRatio     = *apvts.getRawParameterValue ("SUB_RATIO");

        // Calculate current playing index based on individual sequence length loops
        int mainCurrentStep = (startStepIndex % mainMaxSteps) + 1;
        int subCurrentStep  = (startStepIndex % subMaxSteps) + 1;

        juce::String mainStepId = juce::String (mainCurrentStep);
        juce::String subStepId  = juce::String (subCurrentStep);

        // Extract Step parameter data from APVTS
        int mainNote      = *apvts.getRawParameterValue ("MAIN_NOTE_" + mainStepId);
        float mainVel     = *apvts.getRawParameterValue ("MAIN_VEL_" + mainStepId);
        float mainLen     = *apvts.getRawParameterValue ("MAIN_LEN_" + mainStepId);

        bool subActive    = *apvts.getRawParameterValue ("SUB_ACTIVE_" + subStepId) > 0.5f;
        float subVel      = *apvts.getRawParameterValue ("SUB_VEL_" + subStepId);
        float subLen      = *apvts.getRawParameterValue ("SUB_LEN_" + subStepId);

        // Find precise step trigger point relative to the buffer start
        double targetTriggerPpq = startStepIndex * stepDurationPpq;
        int sampleOffset = 0;
        
        if (targetTriggerPpq > startPpq)
        {
            double ratio = (targetTriggerPpq - startPpq) / (endPpq - startPpq);
            sampleOffset = juce::jlimit (0, int (getSampleRate() / 20.0), int (ratio * getSampleRate()));
        }

        // --- Trigger Main Note ---
        auto mainOn = juce::MidiMessage::noteOn (1, mainNote, juce::uint8 (mainVel * 127.0f));
        midiMessages.addEvent (mainOn, sampleOffset);
        
        // Calculate the note off boundary in PPQ and store it
        double mainOffPpq = targetTriggerPpq + (stepDurationPpq * mainLen);
        activeNoteOffs.push_back ({ mainNote, mainOffPpq });

        // --- Trigger Subharmonic Note ---
        // The subharmonic tracker shifts step lengths independently but links base pitch to the active main step note!
        if (subActive)
        {
            int subNote = calculateSubharmonicNote (mainNote, subRatio);
            
            auto subOn = juce::MidiMessage::noteOn (1, subNote, juce::uint8 (subVel * 127.0f));
            midiMessages.addEvent (subOn, sampleOffset);

            double subOffPpq = targetTriggerPpq + (stepDurationPpq * subLen);
            activeNoteOffs.push_back ({ subNote, subOffPpq });
        }
    }
}

// State-saving Boilerplate (allows DAWs to cleanly save your sequencer patterns)
void SubharmonicSequencerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SubharmonicSequencerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType())) {
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
    }
}

// Boilerplate Editor linking
juce::AudioProcessorEditor* SubharmonicSequencerAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

// Component creation fallback entrypoint
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SubharmonicSequencerAudioProcessor();
}
