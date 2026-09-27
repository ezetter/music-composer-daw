#include "InstrumentHost.h"

#include "MeasureContent.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr int midiChannel = 1;
    constexpr float scoreVelocity = 0.8f;

    // A chord's notes sound this far apart, from the bottom up: a little for a block chord, so
    // it doesn't sound machine-struck, and more for a rolled one.
    constexpr double blockChordSpread = 0.012;
    constexpr double rolledChordSpread = 0.06;

    // Chords stop this long before the end of their measure.
    constexpr double chordRelease = 0.1;

    /** A note sounding from one time to another, in seconds. */
    struct Sounding
    {
        int noteNumber;
        double start;
        double end;
    };

    void addChord (std::vector<Sounding>& soundings, std::vector<music::Tone> tones, double start, double end, bool rolled)
    {
        std::sort (tones.begin(), tones.end(), [] (const music::Tone& a, const music::Tone& b) { return a.midi < b.midi; });

        const auto spread = rolled ? rolledChordSpread : blockChordSpread;

        for (size_t i = 0; i < tones.size(); ++i)
            soundings.push_back ({ tones[i].midi, start + (double) i * spread, end });
    }
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
void InstrumentHost::play (const Score& score, int firstMeasure, int lastMeasure)
{
    double currentSampleRate = 0.0;

    {
        const juce::ScopedLock sl (lock);
        currentSampleRate = sampleRate;
    }

    if (currentSampleRate <= 0.0)
        return;

    auto events = createNoteEvents (score, firstMeasure, lastMeasure, currentSampleRate);
    const auto beats = score.getBeatsPerMeasure();
    const auto seconds = (double) (lastMeasure - firstMeasure + 1) * score.getSecondsPerMeasure();

    const juce::ScopedLock sl (lock);

    noteEvents.swap (events);
    nextNoteEvent = 0;
    position = 0;
    endPosition = (int64_t) std::llround (seconds * sampleRate);
    secondsPerBeat = score.getSecondsPerMeasure() / beats;
    beatsPerMeasure = beats;
    firstBeat = (double) (firstMeasure * beats);
    playing = true;
    releaseScoreNotes = true;
    playbackPosition = firstBeat;
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

std::vector<InstrumentHost::NoteEvent> InstrumentHost::createNoteEvents (const Score& score, int firstMeasure, int lastMeasure,
                                                                         double sampleRate)
{
    const auto measureSeconds = score.getSecondsPerMeasure();
    const auto beatSeconds = measureSeconds / score.getBeatsPerMeasure();
    std::vector<Sounding> soundings;

    for (auto measure = firstMeasure; measure <= lastMeasure; ++measure)
    {
        const auto start = (double) (measure - firstMeasure) * measureSeconds;
        const auto chordEnd = start + measureSeconds - chordRelease;
        const auto content = getMeasureContent (score, measure);

        for (const auto& staff : content.staves)
        {
            for (const auto& event : staff.events)
            {
                const auto onset = start + event.onset * beatSeconds;

                switch (staff.source)
                {
                    case StaffContent::Source::notes:
                        for (const auto& tone : event.tones)
                            soundings.push_back ({ tone.midi, onset, onset + beatSeconds });
                        break;

                    case StaffContent::Source::chord:
                        if (music::isMelodic (content.chordStyle.type))
                        {
                            // Arpeggio notes ring on to the end of the measure, as with the sustain pedal down.
                            for (const auto& tone : event.tones)
                                soundings.push_back ({ tone.midi, onset, chordEnd });
                        }
                        else
                        {
                            addChord (soundings, event.tones, onset, chordEnd, event.rolled);
                        }
                        break;

                    case StaffContent::Source::alternate:
                        addChord (soundings, event.tones, onset, chordEnd, event.rolled);
                        break;
                }
            }
        }
    }

    // Every note lasts a moment at least, but one that starts again while it's still sounding is
    // cut off first.
    std::sort (soundings.begin(), soundings.end(), [] (const Sounding& a, const Sounding& b)
    {
        return a.noteNumber != b.noteNumber ? a.noteNumber < b.noteNumber : a.start < b.start;
    });

    for (size_t i = 0; i < soundings.size(); ++i)
    {
        auto& sounding = soundings[i];
        sounding.end = juce::jmax (sounding.end, sounding.start + 0.05);

        if (i + 1 < soundings.size() && soundings[i + 1].noteNumber == sounding.noteNumber)
            sounding.end = juce::jmin (sounding.end, soundings[i + 1].start);
    }

    std::vector<NoteEvent> events;

    for (const auto& sounding : soundings)
    {
        const auto startSample = (int64_t) std::llround (sounding.start * sampleRate);
        const auto endSample = (int64_t) std::llround (sounding.end * sampleRate);

        // A note cut off before it starts doesn't play at all.
        if (endSample <= startSample)
            continue;

        events.push_back ({ startSample, sounding.noteNumber, true });
        events.push_back ({ endSample, sounding.noteNumber, false });
    }

    // When a note ends just as the same note starts again, it has to stop before it restarts.
    std::stable_sort (events.begin(), events.end(), [] (const NoteEvent& a, const NoteEvent& b)
    {
        if (a.sample != b.sample)
            return a.sample < b.sample;

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
    const auto beats = getBeatsPlayed();
    const auto beatsPerBar = (double) beatsPerMeasure;

    PositionInfo info;
    info.setIsPlaying (playing);
    info.setBpm (60.0 / secondsPerBeat);
    info.setTimeSignature (TimeSignature { beatsPerMeasure, 4 });
    info.setPpqPosition (beats);
    info.setPpqPositionOfLastBarStart (std::floor (beats / beatsPerBar) * beatsPerBar);
    info.setTimeInSeconds (beats * secondsPerBeat);
    info.setTimeInSamples ((int64_t) std::llround (beats * secondsPerBeat * sampleRate));
    return info;
}

double InstrumentHost::getBeatsPlayed() const noexcept
{
    return firstBeat + (double) position / (sampleRate * secondsPerBeat);
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
        const auto samplePosition = event.sample - position;

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

    if (position >= endPosition)
    {
        playing = false;
        releaseScoreNotes = true;
    }

    playbackPosition = playing ? getBeatsPlayed() : -1.0;
}
