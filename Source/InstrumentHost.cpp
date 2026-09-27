#include "InstrumentHost.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace
{
    constexpr int midiChannel = 1;
    constexpr float scoreVelocity = 0.8f;
}

void InstrumentHost::setInstrument (std::unique_ptr<juce::AudioPluginInstance> newInstrument)
{
    double currentSampleRate = 0.0;
    int currentBlockSize = 0;

    {
        const juce::ScopedLock sl (lock);
        currentSampleRate = sampleRate;
        currentBlockSize = blockSize;
    }

    if (newInstrument != nullptr)
    {
        newInstrument->setPlayHead (this);

        if (currentSampleRate > 0.0)
        {
            newInstrument->setRateAndBufferSizeDetails (currentSampleRate, currentBlockSize);
            newInstrument->prepareToPlay (currentSampleRate, currentBlockSize);
        }
    }

    {
        const juce::ScopedLock sl (lock);
        std::swap (instrument, newInstrument);
        allocateInstrumentBuffer();
    }

    // The old instrument is released and deleted outside the lock, so the audio thread isn't held up.
    if (newInstrument != nullptr)
        newInstrument->releaseResources();
}

double InstrumentHost::getSampleRate() const
{
    const juce::ScopedLock sl (lock);
    return sampleRate > 0.0 ? sampleRate : 44100.0;
}

int InstrumentHost::getBlockSize() const
{
    const juce::ScopedLock sl (lock);
    return blockSize > 0 ? blockSize : 512;
}

//==============================================================================
void InstrumentHost::play (const Score& score)
{
    auto events = createNoteEvents (score);

    const juce::ScopedLock sl (lock);

    if (sampleRate <= 0.0)
        return;

    noteEvents.swap (events);
    nextNoteEvent = 0;
    scoreLength = (double) (score.getNumMeasures() * Score::beatsPerMeasure);
    position = 0;
    playing = true;
    releaseScoreNotes = true;
    playbackPosition = 0.0;
}

void InstrumentHost::stop()
{
    const juce::ScopedLock sl (lock);

    playing = false;
    releaseScoreNotes = true;
    playbackPosition = -1.0;
}

std::optional<double> InstrumentHost::getPlaybackPosition() const noexcept
{
    if (const auto beats = playbackPosition.load(); beats >= 0.0)
        return beats;

    return {};
}

std::vector<InstrumentHost::NoteEvent> InstrumentHost::createNoteEvents (const Score& score)
{
    std::vector<NoteEvent> events;

    for (int measure = 0; measure < score.getNumMeasures(); ++measure)
    {
        for (int beat = 0; beat < Score::beatsPerMeasure; ++beat)
        {
            // A note that's on both staves only plays once.
            std::set<int> noteNumbers;

            for (auto staff : { Staff::treble, Staff::bass })
                for (auto pitch : score.getChord (staff, measure, beat))
                    noteNumbers.insert (getMidiNoteNumber (pitch));

            const auto start = (double) (measure * Score::beatsPerMeasure + beat);

            for (auto noteNumber : noteNumbers)
            {
                events.push_back ({ start, noteNumber, true });
                events.push_back ({ start + 1.0, noteNumber, false });
            }
        }
    }

    // When a note ends just as the same note starts again, it has to stop before it restarts.
    std::stable_sort (events.begin(), events.end(), [] (const NoteEvent& a, const NoteEvent& b)
    {
        if (a.beat < b.beat) return true;
        if (b.beat < a.beat) return false;

        return ! a.isNoteOn && b.isNoteOn;
    });

    return events;
}

//==============================================================================
void InstrumentHost::prepareToPlay (double newSampleRate, int maximumBlockSize)
{
    const juce::ScopedLock sl (lock);

    sampleRate = newSampleRate;
    blockSize = maximumBlockSize;
    midiBuffer.ensureSize (4096);

    if (instrument != nullptr)
    {
        instrument->setRateAndBufferSizeDetails (sampleRate, blockSize);
        instrument->prepareToPlay (sampleRate, blockSize);
        allocateInstrumentBuffer();
    }
}

void InstrumentHost::releaseResources()
{
    const juce::ScopedLock sl (lock);

    if (instrument != nullptr)
        instrument->releaseResources();

    sampleRate = 0.0;
    playing = false;
    scoreNotesOn.reset();
    playbackPosition = -1.0;
}

void InstrumentHost::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepareToPlay (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void InstrumentHost::audioDeviceStopped()
{
    releaseResources();
}

void InstrumentHost::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                       float* const* outputChannelData, int numOutputChannels,
                                                       int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    juce::AudioBuffer<float> output (outputChannelData, numOutputChannels, numSamples);
    output.clear();

    const juce::ScopedLock sl (lock);

    midiBuffer.clear();
    addScoreEvents (midiBuffer, numSamples);

    // This adds the notes played on the on-screen keyboard, and shows the score's notes on it.
    keyboardState.processNextMidiBuffer (midiBuffer, 0, numSamples, true);

    if (instrument != nullptr)
    {
        const juce::ScopedLock instrumentLock (instrument->getCallbackLock());

        if (! instrument->isSuspended())
        {
            instrumentBuffer.setSize (instrumentBuffer.getNumChannels(), numSamples, false, false, true);
            instrumentBuffer.clear();
            instrument->processBlock (instrumentBuffer, midiBuffer);

            // A mono instrument plays through every output channel.
            if (const auto numInstrumentOutputs = instrument->getTotalNumOutputChannels(); numInstrumentOutputs > 0)
                for (int channel = 0; channel < numOutputChannels; ++channel)
                    output.copyFrom (channel, 0, instrumentBuffer, juce::jmin (channel, numInstrumentOutputs - 1), 0, numSamples);
        }
    }

    advancePlayback (numSamples);
}

//==============================================================================
juce::Optional<juce::AudioPlayHead::PositionInfo> InstrumentHost::getPosition() const
{
    const auto beats = (double) position / getSamplesPerBeat();
    const auto beatsPerMeasure = (double) Score::beatsPerMeasure;

    PositionInfo info;
    info.setIsPlaying (playing);
    info.setBpm (tempo);
    info.setTimeSignature (TimeSignature { Score::beatsPerMeasure, 4 });
    info.setPpqPosition (beats);
    info.setPpqPositionOfLastBarStart (std::floor (beats / beatsPerMeasure) * beatsPerMeasure);
    info.setTimeInSeconds ((double) position / sampleRate);
    info.setTimeInSamples (position);
    return info;
}

double InstrumentHost::getSamplesPerBeat() const noexcept
{
    return sampleRate * 60.0 / tempo;
}

int64_t InstrumentHost::beatsToSamples (double beats) const noexcept
{
    return (int64_t) std::llround (beats * getSamplesPerBeat());
}

void InstrumentHost::allocateInstrumentBuffer()
{
    if (instrument != nullptr)
        instrumentBuffer.setSize (juce::jmax (instrument->getTotalNumInputChannels(), instrument->getTotalNumOutputChannels()),
                                  juce::jmax (blockSize, 1));
}

void InstrumentHost::addScoreEvents (juce::MidiBuffer& midi, int numSamples)
{
    if (releaseScoreNotes)
    {
        for (int noteNumber = 0; noteNumber < (int) scoreNotesOn.size(); ++noteNumber)
            if (scoreNotesOn[(size_t) noteNumber])
                midi.addEvent (juce::MidiMessage::noteOff (midiChannel, noteNumber), 0);

        scoreNotesOn.reset();
        releaseScoreNotes = false;
    }

    if (! playing)
        return;

    for (; nextNoteEvent < noteEvents.size(); ++nextNoteEvent)
    {
        const auto& event = noteEvents[nextNoteEvent];
        const auto samplePosition = beatsToSamples (event.beat) - position;

        if (samplePosition >= numSamples)
            break;

        midi.addEvent (event.isNoteOn ? juce::MidiMessage::noteOn (midiChannel, event.noteNumber, scoreVelocity)
                                      : juce::MidiMessage::noteOff (midiChannel, event.noteNumber),
                       (int) juce::jmax ((int64_t) 0, samplePosition));

        scoreNotesOn[(size_t) event.noteNumber] = event.isNoteOn;
    }
}

void InstrumentHost::advancePlayback (int numSamples)
{
    if (! playing)
        return;

    position += numSamples;

    if (position >= beatsToSamples (scoreLength))
    {
        playing = false;
        releaseScoreNotes = true;
    }

    playbackPosition = playing ? (double) position / getSamplesPerBeat() : -1.0;
}
