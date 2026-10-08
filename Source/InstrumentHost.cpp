#include "InstrumentHost.h"

#include "MeasureContent.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace
{
    constexpr int midiChannel = 1;

    // How loud the metronome is, at its loudest
    constexpr float clickLevel = 0.32f;
    constexpr float clangLevel = 0.36f;

    // A chord's notes sound this far apart, from the bottom up: a little for a block chord, so
    // it doesn't sound machine-struck, and more for a rolled one.
    constexpr double blockChordSpread = 0.012;
    constexpr double rolledChordSpread = 0.06;


    /** A note sounding from one time to another, in seconds. */
    struct Sounding
    {
        int noteNumber;
        double start;
        double end;
        int velocity;
    };

    void addChord (std::vector<Sounding>& soundings, std::vector<music::Tone> tones, double start, double end, bool rolled, int velocity)
    {
        std::sort (tones.begin(), tones.end(), [] (const music::Tone& a, const music::Tone& b) { return a.midi < b.midi; });

        const auto spread = rolled ? rolledChordSpread : blockChordSpread;

        for (size_t i = 0; i < tones.size(); ++i)
            soundings.push_back ({ tones[i].midi, start + (double) i * spread, end, velocity });
    }
}

InstrumentHost::InstrumentHost()
{
    playedMidi.ensureSize (4096);

    for (int part = 0; part < Score::initialParts; ++part)
        insertPart (part);
}

void InstrumentHost::insertPart (int index)
{
    auto slot = std::make_unique<Slot>();

    {
        const juce::ScopedLock sl (lock);
        jassert (juce::isPositiveAndNotGreaterThan (index, (int) slots.size()));

        slot->midi.ensureSize (4096);

        if (sampleRate > 0.0)
            slot->gain.reset (sampleRate, 0.05);

        // The part the keyboard's playing moves down, if it was there or after.
        if (! slots.empty() && keyboardPart >= index)
            ++keyboardPart;

        slots.insert (slots.begin() + index, std::move (slot));
        nextNoteEvents.insert (nextNoteEvents.begin() + index, 0);

        // Whatever's playing has nothing for it.
        if (index <= (int) passage.events.size())
            passage.events.insert (passage.events.begin() + index, std::vector<NoteEvent>());
    }
}

void InstrumentHost::removePart (int index)
{
    std::unique_ptr<Slot> removed;

    {
        const juce::ScopedLock sl (lock);
        jassert (juce::isPositiveAndBelow (index, (int) slots.size()));

        removed = std::move (slots[(size_t) index]);
        slots.erase (slots.begin() + index);
        nextNoteEvents.erase (nextNoteEvents.begin() + index);

        if (index < (int) passage.events.size())
            passage.events.erase (passage.events.begin() + index);

        // The keyboard moves on to whichever part's active now, which the caller chooses.
        if (keyboardPart > index || keyboardPart >= (int) slots.size())
            keyboardPart = juce::jmax (0, keyboardPart - 1);

        activePart = juce::jlimit (0, juce::jmax (0, (int) slots.size() - 1), activePart.load() > index ? activePart.load() - 1 : activePart.load());
    }

    // Its instrument is released and deleted outside the lock, so the audio thread isn't held up.
    if (removed->instrument != nullptr)
        removed->instrument->releaseResources();
}

void InstrumentHost::setInstrument (int part, std::unique_ptr<juce::AudioPluginInstance> newInstrument)
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
        auto& slot = *slots[(size_t) part];
        std::swap (slot.instrument, newInstrument);
        allocateInstrumentBuffer (slot);
    }

    // The old instrument is released and deleted outside the lock, so the audio thread isn't held up.
    if (newInstrument != nullptr)
        newInstrument->releaseResources();
}

void InstrumentHost::setActivePart (int part)
{
    jassert (juce::isPositiveAndBelow (part, getNumParts()));

    // The audio thread notices, and lets go of the keyboard's notes on the other instrument.
    activePart = part;
}

void InstrumentHost::setVolume (int part, float decibels)
{
    slots[(size_t) part]->volume = juce::jlimit (minVolume, maxVolume, decibels);
}

void InstrumentHost::setMuted (int part, bool shouldBeMuted)
{
    slots[(size_t) part]->muted = shouldBeMuted;
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
void InstrumentHost::play (const Score& score, int firstMeasure, int lastMeasure, bool loop, double startBeat)
{
    double currentSampleRate = 0.0;

    {
        const juce::ScopedLock sl (lock);
        currentSampleRate = sampleRate;
    }

    if (currentSampleRate <= 0.0)
        return;

    auto newPassage = createPassage (score, firstMeasure, lastMeasure, currentSampleRate);

    {
        const juce::ScopedLock sl (lock);

        std::swap (passage, newPassage);
        std::fill (nextNoteEvents.begin(), nextNoteEvents.end(), 0);
        nextBeat = 0;
        position = 0;
        nextPosition = 0;
        looping = loop;
        playing = true;

        // From the start, or part way through: the notes that were sounding stop, and the pedal
        // goes where the score has it there.
        position = juce::jlimit ((int64_t) 0, passage.length,
                                 (int64_t) std::llround ((startBeat - passage.firstBeat) * passage.secondsPerBeat * sampleRate));
        nextPosition = position;
        catchUp (false);
    }

    // The old passage is freed here, outside the lock.
}

void InstrumentHost::updatePlaying (const Score& score, int firstMeasure, int lastMeasure, std::optional<double> beat)
{
    double currentSampleRate = 0.0;

    {
        const juce::ScopedLock sl (lock);

        if (! playing)
            return;

        currentSampleRate = sampleRate;
    }

    auto newPassage = createPassage (score, firstMeasure, lastMeasure, currentSampleRate);

    {
        const juce::ScopedLock sl (lock);

        if (! playing)
            return;

        // The beat asked for, or the same beat, at the new tempo
        if (beat.has_value())
            position = juce::jlimit ((int64_t) 0, newPassage.length,
                                     (int64_t) std::llround ((*beat - newPassage.firstBeat) * newPassage.secondsPerBeat * sampleRate));
        else if (passage.secondsPerBeat > 0.0)
            position = (int64_t) std::llround ((double) position * newPassage.secondsPerBeat / passage.secondsPerBeat);

        nextPosition = position;
        std::swap (passage, newPassage);
        catchUp (true);
    }

    // The old passage is freed here, outside the lock.
}

void InstrumentHost::jumpTo (double beat)
{
    const juce::ScopedLock sl (lock);

    if (! playing)
        return;

    position = juce::jlimit ((int64_t) 0, passage.length,
                             (int64_t) std::llround ((beat - passage.firstBeat) * passage.secondsPerBeat * sampleRate));
    nextPosition = position;
    catchUp (false);
}

void InstrumentHost::catchUp (bool keepNotesSounding)
{
    // The metronome carries on from the next beat.
    for (nextBeat = 0; nextBeat < passage.beats.size() && passage.beats[nextBeat].sample < position; ++nextBeat) {}

    // Each part carries on from its first event that hasn't been played yet. What it's played
    // so far is made to match: notes sounding that it wouldn't have stop, or all of them, and the
    // pedal goes where it would be.
    for (size_t part = 0; part < slots.size(); ++part)
    {
        auto& slot = *slots[part];
        auto& next = nextNoteEvents[part];
        std::bitset<128> sounding;
        auto pedalDown = false;
        next = 0;

        if (part < passage.events.size())
        {
            for (const auto& events = passage.events[part]; next < events.size() && events[next].sample < position; ++next)
            {
                if (events[next].isPedal)
                    pedalDown = events[next].isNoteOn;
                else
                    sounding[(size_t) events[next].noteNumber] = events[next].isNoteOn;
            }
        }

        if (! keepNotesSounding)
            sounding.reset();

        slot.pendingNoteOffs |= slot.scoreNotesOn & ~sounding;
        slot.scoreNotesOn &= sounding;

        if (slot.scorePedalDown != pedalDown)
        {
            slot.pendingPedal = pedalDown ? 1 : 0;
            slot.scorePedalDown = pedalDown;
        }
    }

    playbackPosition = getBeatsPlayed();
}

InstrumentHost::Passage InstrumentHost::createPassage (const Score& score, int firstMeasure, int lastMeasure, double sampleRate)
{
    Passage result;

    for (int part = 0; part < score.getNumParts(); ++part)
        result.events.push_back (createNoteEvents (score, part, firstMeasure, lastMeasure, sampleRate));

    result.length = (int64_t) std::llround ((double) (lastMeasure - firstMeasure + 1) * score.getSecondsPerMeasure() * sampleRate);
    result.beatsPerMeasure = score.getBeatsPerMeasure();
    result.secondsPerBeat = score.getSecondsPerMeasure() / result.beatsPerMeasure;
    result.firstBeat = (double) (firstMeasure * result.beatsPerMeasure);

    // The metronome's beats, the first of each measure its downbeat
    for (int beat = 0;; ++beat)
    {
        const auto sample = (int64_t) std::llround ((double) beat * result.secondsPerBeat * sampleRate);

        if (sample >= result.length)
            break;

        result.beats.push_back ({ sample, beat % result.beatsPerMeasure == 0 });
    }

    // Nothing sounds past the end, so a loop's notes are all finished before it starts again.
    for (auto& events : result.events)
        for (auto& event : events)
            event.sample = juce::jmin (event.sample, result.length);

    return result;
}

void InstrumentHost::setLooping (bool shouldLoop)
{
    const juce::ScopedLock sl (lock);
    looping = shouldLoop;
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

std::vector<InstrumentHost::NoteEvent> InstrumentHost::createNoteEvents (const Score& score, int part, int firstMeasure, int lastMeasure,
                                                                         double sampleRate)
{
    const auto measureSeconds = score.getSecondsPerMeasure();
    const auto beatSeconds = measureSeconds / score.getBeatsPerMeasure();
    std::vector<Sounding> soundings;

    // Notes tied to the next note of the same pitch, by staff and pitch, waiting for that note
    // to hold them on through it, rather than playing it again
    std::map<std::pair<size_t, int>, size_t> tiedOn;

    for (auto measure = firstMeasure; measure <= lastMeasure; ++measure)
    {
        const auto start = (double) (measure - firstMeasure) * measureSeconds;
        const auto content = getMeasureContent (score, part, measure);

        for (size_t staffIndex = 0; staffIndex < content.staves.size(); ++staffIndex)
        {
            const auto& staff = content.staves[staffIndex];

            for (const auto& event : staff.events)
            {
                // Every note is held for exactly as long as it's written: a beat for a quarter note,
                // four for a whole note, whatever wrote it. A chord written as a whole note lasts
                // the measure because that's how long a whole note is in 4/4.
                const auto onset = start + event.onset * beatSeconds;
                const auto end = onset + getBeats (event.duration) * beatSeconds;

                // As loud as the dynamics and hairpins before it make it, wherever playing started
                const auto velocity = score.getVelocity (part, measure, event.onset);

                // A block or rolled chord, the chord's or the alternate staff's, is spread from the
                // bottom up as it starts; everything else starts together.
                const auto spreadAsChord = staff.source == StaffContent::Source::alternate
                                        || (staff.source == StaffContent::Source::chord && ! music::isMelodic (content.chordStyle.type));

                if (spreadAsChord)
                {
                    addChord (soundings, event.tones, onset, end, event.rolled, velocity);
                    continue;
                }

                for (const auto& tone : event.tones)
                {
                    const auto key = std::pair { staffIndex, tone.midi };
                    size_t index;

                    // A note a tie leads to holds the note before it on, to its own end, as loud as it started.
                    if (const auto held = tiedOn.find (key); held != tiedOn.end() && std::abs (soundings[held->second].end - onset) < 1.0e-9)
                    {
                        index = held->second;
                        soundings[index].end = end;
                        tiedOn.erase (held);
                    }
                    else
                    {
                        index = soundings.size();
                        soundings.push_back ({ tone.midi, onset, end, velocity });
                    }

                    const auto& tied = staff.tiedNotes;

                    if (std::any_of (tied.begin(), tied.end(), [&] (const auto& t) { return juce::exactlyEqual (t.first, event.onset) && t.second == tone.pitch; }))
                        tiedOn[key] = index;
                    else
                        tiedOn.erase (key);
                }
            }
        }
    }

    // While the sustain pedal is down, a note that's still sounding as it goes down, or that ends
    // before it comes up, is held until it comes up.
    const auto firstBeat = (double) (firstMeasure * score.getBeatsPerMeasure());

    for (const auto& pedal : score.getPedals (part))
    {
        const auto down = (pedal.start - firstBeat) * beatSeconds;
        const auto up = (pedal.end - firstBeat) * beatSeconds;

        for (auto& sounding : soundings)
            if (sounding.end > down + 1.0e-9 && sounding.end < up - 1.0e-9)
                sounding.end = up;
    }

    // A note that starts again while it's still sounding is cut off first, as one key can't be
    // down twice.
    std::sort (soundings.begin(), soundings.end(), [] (const Sounding& a, const Sounding& b)
    {
        return a.noteNumber != b.noteNumber ? a.noteNumber < b.noteNumber : a.start < b.start;
    });

    for (size_t i = 0; i < soundings.size(); ++i)
    {
        auto& sounding = soundings[i];

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

        events.push_back ({ startSample, sounding.noteNumber, true, (juce::uint8) sounding.velocity });
        events.push_back ({ endSample, sounding.noteNumber, false });
    }

    // The instrument hears the sustain pedal too, as well as the notes being held for it: down
    // from where it goes down, or the start if it's down already, until it comes up, or the end.
    // Lifted and put straight down again, it goes down a moment after it comes up, so the
    // instrument hears it lift.
    constexpr double relift = 0.05;
    const auto passageSeconds = (double) (lastMeasure - firstMeasure + 1) * measureSeconds;
    const auto pedals = score.getPedals (part);

    for (size_t i = 0; i < pedals.size(); ++i)
    {
        const auto& pedal = pedals[i];
        const auto liftedJustBefore = i > 0 && pedals[i - 1].lifted && juce::exactlyEqual (pedals[i - 1].end, pedal.start);
        const auto down = (pedal.start - firstBeat) * beatSeconds + (liftedJustBefore ? relift : 0.0);
        const auto up = (pedal.end - firstBeat) * beatSeconds;

        if (up <= 0.0 || down >= passageSeconds || down >= up)
            continue;

        events.push_back ({ (int64_t) std::llround (juce::jmax (0.0, down) * sampleRate), -1, true, 127, true });
        events.push_back ({ (int64_t) std::llround (juce::jmin (up, passageSeconds) * sampleRate), -1, false, 0, true });
    }

    // At the same moment, notes stop first, then the pedal comes up and goes down again, then
    // notes start: so a note that ends just as the same note starts again stops before it
    // restarts, and the pedal catches the notes starting with it, not the ones ending.
    const auto order = [] (const NoteEvent& e) { return e.isPedal ? (e.isNoteOn ? 2 : 1) : (e.isNoteOn ? 3 : 0); };

    std::stable_sort (events.begin(), events.end(), [&order] (const NoteEvent& a, const NoteEvent& b)
    {
        if (a.sample != b.sample)
            return a.sample < b.sample;

        return order (a) < order (b);
    });

    return events;
}

//==============================================================================
void InstrumentHost::prepareToPlay (double newSampleRate, int maximumBlockSize)
{
    const juce::ScopedLock sl (lock);

    sampleRate = newSampleRate;
    blockSize = maximumBlockSize;
    midiInput.reset (newSampleRate);
    clickSound = createClick (sampleRate);
    clangSound = createClang (sampleRate);
    metronomeVoices = {};
    midiInputReady = true;

    for (auto& slotPointer : slots)
    {
        auto& slot = *slotPointer;
        slot.midi.ensureSize (4096);
        slot.gain.reset (sampleRate, 0.05);
        slot.gain.setCurrentAndTargetValue (slot.muted ? 0.0f : juce::Decibels::decibelsToGain (slot.volume.load(), minVolume));

        if (slot.instrument != nullptr)
        {
            slot.instrument->setRateAndBufferSizeDetails (sampleRate, blockSize);
            slot.instrument->prepareToPlay (sampleRate, blockSize);
            allocateInstrumentBuffer (slot);
        }
    }
}

void InstrumentHost::releaseResources()
{
    const juce::ScopedLock sl (lock);

    for (auto& slot : slots)
    {
        if (slot->instrument != nullptr)
            slot->instrument->releaseResources();

        slot->scoreNotesOn.reset();
    }

    sampleRate = 0.0;
    midiInputReady = false;
    playing = false;
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

    // While one score's parts make way for another's, there can briefly be none.
    if (slots.empty())
        return;

    for (auto& slot : slots)
        slot->midi.clear();

    // The keyboard's notes stop on the instrument it was playing when another part becomes active.
    if (const auto part = activePart.load(); part != keyboardPart)
    {
        letGoOfKeyboardNotes (*slots[(size_t) keyboardPart]);
        keyboardPart = part;
    }

    auto& keyboardSlot = *slots[(size_t) keyboardPart];
    addScoreEvents (numSamples);

    // What's played: the MIDI controllers' notes, which show on the on-screen keyboard, and the
    // notes played on it. Kept apart from the score's notes, it can be recorded.
    playedMidi.clear();
    midiInput.removeNextBlockOfMessages (playedMidi, numSamples);
    keyboardState.processNextMidiBuffer (playedMidi, 0, numSamples, true);
    recordKeys (playedMidi);

    // The active part's notes from the score show on the keyboard too.
    keyboardState.processNextMidiBuffer (keyboardSlot.midi, 0, numSamples, false);
    keyboardSlot.midi.addEvents (playedMidi, 0, numSamples, 0);

    for (auto& slotPointer : slots)
    {
        auto& slot = *slotPointer;
        auto* instrument = slot.instrument.get();

        if (instrument == nullptr)
            continue;

        const juce::ScopedLock instrumentLock (instrument->getCallbackLock());

        if (instrument->isSuspended())
            continue;

        slot.buffer.setSize (slot.buffer.getNumChannels(), numSamples, false, false, true);
        slot.buffer.clear();
        instrument->processBlock (slot.buffer, slot.midi);

        slot.gain.setTargetValue (slot.muted ? 0.0f : juce::Decibels::decibelsToGain (slot.volume.load(), minVolume));
        slot.gain.applyGain (slot.buffer, numSamples);

        // The instruments are mixed together, each at its volume, a mono one playing through every
        // output channel.
        if (const auto numInstrumentOutputs = instrument->getTotalNumOutputChannels(); numInstrumentOutputs > 0)
            for (int channel = 0; channel < numOutputChannels; ++channel)
                output.addFrom (channel, 0, slot.buffer, juce::jmin (channel, numInstrumentOutputs - 1), 0, numSamples);
    }

    addMetronome (output, numSamples);
    advancePlayback();
}

void InstrumentHost::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    // Until the audio has started there's nothing to play it, and nowhere for it to wait.
    if (midiInputReady)
        midiInput.addMessageToQueue (message);
}

//==============================================================================
juce::Optional<juce::AudioPlayHead::PositionInfo> InstrumentHost::getPosition() const
{
    const auto beats = getBeatsPlayed();
    const auto beatsPerBar = (double) passage.beatsPerMeasure;
    const auto secondsPerBeat = passage.secondsPerBeat;

    PositionInfo info;
    info.setIsPlaying (playing);
    info.setBpm (60.0 / secondsPerBeat);
    info.setTimeSignature (TimeSignature { passage.beatsPerMeasure, 4 });
    info.setPpqPosition (beats);
    info.setPpqPositionOfLastBarStart (std::floor (beats / beatsPerBar) * beatsPerBar);
    info.setTimeInSeconds (beats * secondsPerBeat);
    info.setTimeInSamples ((int64_t) std::llround (beats * secondsPerBeat * sampleRate));
    return info;
}

double InstrumentHost::getBeatsPlayed (int64_t samplesIntoBlock) const noexcept
{
    // Past the end of a loop is back at the beginning.
    const auto played = position + samplesIntoBlock;
    const auto samples = looping && passage.length > 0 && played >= passage.length ? played - passage.length : played;
    return passage.firstBeat + (double) samples / (sampleRate * passage.secondsPerBeat);
}

void InstrumentHost::recordKeys (const juce::MidiBuffer& midi)
{
    if (! recording || ! playing)
        return;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (! message.isNoteOnOrOff())
            continue;

        // If the message thread's fallen so far behind there's no room, the key's lost.
        const RecordedKey key { message.getNoteNumber(), message.isNoteOn(), getBeatsPlayed (metadata.samplePosition) };
        recordedKeysFifo.write (1).forEach ([&] (int index) { recordedKeys[(size_t) index] = key; });
    }
}

std::vector<InstrumentHost::RecordedKey> InstrumentHost::takeRecordedKeys()
{
    std::vector<RecordedKey> keys;
    const auto scope = recordedKeysFifo.read (recordedKeysFifo.getNumReady());
    scope.forEach ([&] (int index) { keys.push_back (recordedKeys[(size_t) index]); });
    return keys;
}

void InstrumentHost::allocateInstrumentBuffer (Slot& slot)
{
    if (auto* instrument = slot.instrument.get())
        slot.buffer.setSize (juce::jmax (instrument->getTotalNumInputChannels(), instrument->getTotalNumOutputChannels()),
                             juce::jmax (blockSize, 1));
}

void InstrumentHost::letGoOfKeyboardNotes (Slot& slot)
{
    // The keys that are down, apart from the score's own notes, which play on regardless.
    for (int channel = 1; channel <= 16; ++channel)
    {
        for (int noteNumber = 0; noteNumber < 128; ++noteNumber)
            if (keyboardState.isNoteOn (channel, noteNumber) && ! slot.scoreNotesOn[(size_t) noteNumber])
                slot.midi.addEvent (juce::MidiMessage::noteOff (channel, noteNumber), 0);

        slot.midi.addEvent (juce::MidiMessage::controllerEvent (channel, 64, 0), 0);     // the sustain pedal
    }

    keyboardState.reset();
}

void InstrumentHost::addScoreEvents (int numSamples)
{
    if (releaseScoreNotes)
    {
        for (auto& slot : slots)
        {
            for (int noteNumber = 0; noteNumber < (int) slot->scoreNotesOn.size(); ++noteNumber)
                if (slot->scoreNotesOn[(size_t) noteNumber])
                    slot->midi.addEvent (juce::MidiMessage::noteOff (midiChannel, noteNumber), 0);

            slot->scoreNotesOn.reset();

            if (slot->scorePedalDown)
                slot->midi.addEvent (juce::MidiMessage::controllerEvent (midiChannel, 64, 0), 0);

            slot->scorePedalDown = false;
        }

        releaseScoreNotes = false;
    }

    // Notes the score no longer has where they were sounding, and the pedal, as it's changed
    for (auto& slot : slots)
    {
        for (size_t noteNumber = 0; noteNumber < slot->pendingNoteOffs.size(); ++noteNumber)
            if (slot->pendingNoteOffs[noteNumber])
                slot->midi.addEvent (juce::MidiMessage::noteOff (midiChannel, (int) noteNumber), 0);

        slot->pendingNoteOffs.reset();

        if (slot->pendingPedal >= 0)
        {
            slot->midi.addEvent (juce::MidiMessage::controllerEvent (midiChannel, 64, slot->pendingPedal > 0 ? 127 : 0), 0);
            slot->scorePedalDown = slot->pendingPedal > 0;
        }

        slot->pendingPedal = -1;
    }

    if (! playing)
        return;

    // Where this time through the measures started, relative to the start of the block
    auto start = -position;

    for (;;)
    {
        auto allPlayed = true;

        // A part added since the passage was made has nothing to play yet.
        for (size_t part = 0; part < slots.size() && part < passage.events.size(); ++part)
        {
            auto& slot = *slots[part];
            const auto& events = passage.events[part];
            auto& next = nextNoteEvents[part];

            for (; next < events.size(); ++next)
            {
                const auto& event = events[next];
                const auto samplePosition = start + event.sample;

                if (samplePosition >= numSamples)
                    break;

                const auto at = (int) juce::jmax ((int64_t) 0, samplePosition);

                // The sustain pedal is controller 64: all the way down, or up.
                if (event.isPedal)
                {
                    slot.midi.addEvent (juce::MidiMessage::controllerEvent (midiChannel, 64, event.isNoteOn ? 127 : 0), at);
                    slot.scorePedalDown = event.isNoteOn;
                    continue;
                }

                // A note that didn't start, as playing started after it, isn't stopped, in case
                // the same key's being played.
                if (! event.isNoteOn && ! slot.scoreNotesOn[(size_t) event.noteNumber])
                    continue;

                slot.midi.addEvent (event.isNoteOn ? juce::MidiMessage::noteOn (midiChannel, event.noteNumber, event.velocity)
                                                   : juce::MidiMessage::noteOff (midiChannel, event.noteNumber),
                                    at);

                slot.scoreNotesOn[(size_t) event.noteNumber] = event.isNoteOn;
            }

            allPlayed = allPlayed && next == events.size();
        }

        // The metronome's beats, heard only while it's on
        for (; nextBeat < passage.beats.size(); ++nextBeat)
        {
            const auto& beat = passage.beats[nextBeat];
            const auto samplePosition = start + beat.sample;

            if (samplePosition >= numSamples)
                break;

            if (metronome)
                startMetronomeSound (beat.downbeat, (int) juce::jmax ((int64_t) 0, samplePosition));
        }

        allPlayed = allPlayed && nextBeat == passage.beats.size();

        // When looping, the next time through can start part way through the block, once
        // everything in this one has been played.
        if (! looping || passage.length <= 0 || ! allPlayed || start + passage.length >= numSamples)
            break;

        start += passage.length;
        std::fill (nextNoteEvents.begin(), nextNoteEvents.end(), 0);
        nextBeat = 0;
    }

    nextPosition = numSamples - start;
}

void InstrumentHost::advancePlayback()
{
    if (! playing)
        return;

    // addScoreEvents() has worked out where the next block starts, back at the beginning if it looped.
    position = nextPosition;

    if (! looping && position >= passage.length)
    {
        playing = false;
        releaseScoreNotes = true;
    }

    playbackPosition = playing ? getBeatsPlayed() : -1.0;
}

//==============================================================================
std::vector<float> InstrumentHost::createClick (double rate)
{
    // A woodblock's knock: two high partials dying away in a few milliseconds, over a burst of noise
    std::vector<float> sound ((size_t) std::lround (0.05 * rate));
    juce::Random noise (1);

    for (size_t i = 0; i < sound.size(); ++i)
    {
        const auto t = (double) i / rate;
        const auto tone = 0.65 * std::sin (juce::MathConstants<double>::twoPi * 1900.0 * t)
                        + 0.35 * std::sin (juce::MathConstants<double>::twoPi * 3100.0 * t);
        sound[i] = clickLevel * (float) (tone * std::exp (-t / 0.006)
                                         + 0.35 * (noise.nextDouble() * 2.0 - 1.0) * std::exp (-t / 0.0012));
    }

    return sound;
}

std::vector<float> InstrumentHost::createClang (double rate)
{
    // A sharp clang, like a cowbell's: struck metal that's gone in a moment rather than ringing
    // on. Two tones a little under a fifth apart, as in the classic drum machine cowbell, and
    // higher, inharmonic overtones, all dying away fast, over a burst of noise for the strike, and
    // driven a little hard for a metallic edge.
    struct Partial { double frequency, amplitude, decay; };
    constexpr Partial partials[] { { 830.0, 1.0, 0.06 }, { 1205.0, 0.8, 0.045 }, { 2470.0, 0.45, 0.028 },
                                   { 3540.0, 0.3, 0.018 }, { 5310.0, 0.2, 0.01 } };
    std::vector<float> sound ((size_t) std::lround (0.25 * rate));
    juce::Random noise (2);
    auto peak = 0.0f;

    for (size_t i = 0; i < sound.size(); ++i)
    {
        const auto t = (double) i / rate;
        auto value = 0.0;

        for (const auto& partial : partials)
            value += partial.amplitude * std::sin (juce::MathConstants<double>::twoPi * partial.frequency * t) * std::exp (-t / partial.decay);

        // Loudest as it's struck, settling quickly into what's left of the clang
        value *= 0.55 + 0.45 * std::exp (-t / 0.01);
        value += 0.6 * (noise.nextDouble() * 2.0 - 1.0) * std::exp (-t / 0.0015);
        sound[i] = (float) std::tanh (1.6 * value);
        peak = juce::jmax (peak, std::abs (sound[i]));
    }

    for (auto& sample : sound)
        sample *= clangLevel / peak;

    return sound;
}

void InstrumentHost::startMetronomeSound (bool downbeat, int delay)
{
    const auto* sound = downbeat ? &clangSound : &clickSound;

    if (sound->empty())
        return;

    // A free voice, or else the one that's played longest, which is nearly finished
    auto* voice = &metronomeVoices.front();

    for (auto& candidate : metronomeVoices)
    {
        if (candidate.sound == nullptr)
        {
            voice = &candidate;
            break;
        }

        if (candidate.played > voice->played)
            voice = &candidate;
    }

    *voice = { sound, 0, delay };
}

void InstrumentHost::addMetronome (juce::AudioBuffer<float>& output, int numSamples)
{
    for (auto& voice : metronomeVoices)
    {
        if (voice.sound == nullptr)
            continue;

        const auto& sound = *voice.sound;
        const auto from = juce::jmin (voice.delay, numSamples);
        const auto count = (int) juce::jmin ((size_t) (numSamples - from), sound.size() - voice.played);

        for (int channel = 0; channel < output.getNumChannels(); ++channel)
            output.addFrom (channel, from, sound.data() + voice.played, count);

        voice.played += (size_t) count;
        voice.delay = juce::jmax (0, voice.delay - numSamples);

        if (voice.played >= sound.size())
            voice.sound = nullptr;
    }
}
