#include "Score.h"

#include <algorithm>

namespace
{
    std::vector<int> getMidiNotes (const music::Chord& chord)
    {
        std::vector<int> midiNotes;

        for (const auto& tone : chord.tones)
            if (std::find (midiNotes.begin(), midiNotes.end(), tone.midi) == midiNotes.end())
                midiNotes.push_back (tone.midi);

        return midiNotes;
    }

    bool comesBefore (const music::Pitch& a, const music::Pitch& b)
    {
        return a.step != b.step ? a.step < b.step : a.alter < b.alter;
    }
}

Score::Score() = default;

music::Spelling Score::getKey() const
{
    return music::getMajorKeys()[(size_t) keyIndex];
}

void Score::setKeyIndex (int newKeyIndex)
{
    jassert (juce::isPositiveAndBelow (newKeyIndex, (int) music::getMajorKeys().size()));

    if (newKeyIndex == keyIndex)
        return;

    keyIndex = newKeyIndex;

    // Notes set on the piano keep their pitches, so describe them again in the new key.
    for (auto& measure : measures)
    {
        if (measure.chord.has_value() && measure.chord->keyboardNotes.has_value())
        {
            std::vector<int> midiNotes;

            for (const auto& note : *measure.chord->keyboardNotes)
                midiNotes.push_back (note.midi);

            const auto description = music::describeNotes (getKey(), measure.chord->style.staff, midiNotes);
            measure.chord->spec = description.spec;
            measure.chord->keyboardNotes = description.keyboardNotes;
        }

        tidyChord (measure);
    }

    sendSynchronousChangeMessage();
}

void Score::setBeatsPerMeasure (int newBeatsPerMeasure)
{
    jassert (newBeatsPerMeasure >= 2 && newBeatsPerMeasure <= maxBeatsPerMeasure);

    if (newBeatsPerMeasure == beatsPerMeasure)
        return;

    beatsPerMeasure = newBeatsPerMeasure;

    for (auto& measure : measures)
        tidyChord (measure);

    sendSynchronousChangeMessage();
}

void Score::setBeatsPerMinute (double newBeatsPerMinute)
{
    jassert (newBeatsPerMinute >= minBeatsPerMinute && newBeatsPerMinute <= maxBeatsPerMinute);

    if (juce::exactlyEqual (newBeatsPerMinute, beatsPerMinute))
        return;

    beatsPerMinute = newBeatsPerMinute;
    sendSynchronousChangeMessage();
}

//==============================================================================
void Score::addMeasure()
{
    measures.emplace_back();
    sendSynchronousChangeMessage();
}

void Score::removeLastMeasure()
{
    if (measures.size() <= 1)
        return;

    measures.pop_back();
    sendSynchronousChangeMessage();
}

//==============================================================================
bool Score::addNote (const Note& note)
{
    jassert (juce::isPositiveAndBelow (note.measure, getNumMeasures()));
    jassert (juce::isPositiveAndBelow (note.beat, beatsPerMeasure));

    if (chordUsesStaff (note.measure, note.staff))
        return false;

    auto& notes = measures[(size_t) note.measure].notes[(size_t) note.staff][(size_t) note.beat];
    const auto insertionPoint = std::lower_bound (notes.begin(), notes.end(), note.pitch, comesBefore);

    if (insertionPoint != notes.end() && *insertionPoint == note.pitch)
        return false;

    notes.insert (insertionPoint, note.pitch);
    sendSynchronousChangeMessage();
    return true;
}

const std::vector<music::Pitch>& Score::getNotes (Staff staff, int measure, int beat) const
{
    return measures[(size_t) measure].notes[(size_t) staff][(size_t) beat];
}

bool Score::hasNoteAt (Staff staff, int measure, int beat, int step) const
{
    const auto& notes = getNotes (staff, measure, beat);
    return std::any_of (notes.begin(), notes.end(), [step] (const music::Pitch& pitch) { return pitch.step == step; });
}

bool Score::removeNotesAt (Staff staff, int measure, int beat, int step)
{
    auto& notes = measures[(size_t) measure].notes[(size_t) staff][(size_t) beat];

    if (std::erase_if (notes, [step] (const music::Pitch& pitch) { return pitch.step == step; }) == 0)
        return false;

    sendSynchronousChangeMessage();
    return true;
}

//==============================================================================
const MeasureChord* Score::getChord (int measure) const
{
    const auto& chord = measures[(size_t) measure].chord;
    return chord.has_value() ? &*chord : nullptr;
}

void Score::setChord (int measure, std::optional<MeasureChord> chord)
{
    auto& target = measures[(size_t) measure];
    target.chord = std::move (chord);
    tidyChord (target);
    sendSynchronousChangeMessage();
}

std::optional<music::Chord> Score::getChordNotes (int measure) const
{
    if (const auto* chord = getChord (measure))
        return getChordNotes (*chord);

    return {};
}

std::optional<music::Chord> Score::getChordNotes (const MeasureChord& chord) const
{
    if (chord.keyboardNotes.has_value())
        return music::createChord (getKey(), *chord.keyboardNotes, chord.spec, chord.style.staff);

    if (! chord.spec.isEmpty())
        return music::createChord (getKey(), chord.spec, chord.style.staff);

    return {};
}

bool Score::chordUsesStaff (int measure, Staff staff) const
{
    const auto* chord = getChord (measure);

    return chord != nullptr && chord->hasNotes()
        && (chord->style.staff == staff || chord->style.alternate != music::AlternateStaff::none);
}

void Score::toggleChordNote (int measure, int midiNote, const ChordStyle& styleForNewChord)
{
    auto& target = measures[(size_t) measure];

    if (! target.chord.has_value())
        target.chord = MeasureChord { {}, {}, styleForNewChord, {}, {} };

    std::vector<int> midiNotes;

    if (const auto chord = getChordNotes (*target.chord))
        midiNotes = getMidiNotes (*chord);

    if (const auto existing = std::find (midiNotes.begin(), midiNotes.end(), midiNote); existing != midiNotes.end())
        midiNotes.erase (existing);
    else
        midiNotes.push_back (midiNote);

    const auto description = music::describeNotes (getKey(), target.chord->style.staff, midiNotes);
    target.chord->spec = description.spec;
    target.chord->keyboardNotes = description.keyboardNotes;

    tidyChord (target);
    sendSynchronousChangeMessage();
}

void Score::toggleAlternateNote (int measure, const music::KeyboardNote& note)
{
    auto& target = measures[(size_t) measure];

    if (! target.chord.has_value() || ! target.chord->hasNotes() || target.chord->style.alternate == music::AlternateStaff::none)
        return;

    auto& chord = *target.chord;

    if (! chord.alternateNotes.has_value())
    {
        // Start from the notes the chord gives the alternate staff.
        std::vector<music::KeyboardNote> notes;

        for (const auto& tone : getAlternateTones (measure))
            notes.push_back ({ tone.midi, tone.pitch.getSpelling() });

        chord.alternateNotes = std::move (notes);
    }

    auto& notes = *chord.alternateNotes;

    if (std::erase_if (notes, [&] (const music::KeyboardNote& n) { return n.midi == note.midi; }) == 0)
        notes.push_back (note);

    sendSynchronousChangeMessage();
}

std::vector<music::Tone> Score::getAlternateTones (int measure) const
{
    const auto* chord = getChord (measure);

    if (chord == nullptr || chord->style.alternate == music::AlternateStaff::none)
        return {};

    if (chord->alternateNotes.has_value())
        return music::createTones (getKey(), *chord->alternateNotes);

    if (const auto notes = getChordNotes (measure))
        return music::getAlternateTones (getKey(), *notes, chord->style.staff, chord->style.alternate);

    return {};
}

void Score::reshuffle (int measure)
{
    tidyChord (measures[(size_t) measure], true);
    sendSynchronousChangeMessage();
}

void Score::tidyChord (Measure& measure, bool reshuffleRandomOrder)
{
    if (! measure.chord.has_value())
        return;

    auto& chord = *measure.chord;

    if (! chord.hasNotes())
        return;

    // The chord replaces the quarter notes on the staves it uses.
    for (auto staff : { Staff::treble, Staff::bass })
        if (chord.style.staff == staff || chord.style.alternate != music::AlternateStaff::none)
            for (auto& notes : measure.notes[(size_t) staff])
                notes.clear();

    if (chord.style.type != music::ChordType::random)
        return;

    // A Random chord keeps its order until its notes or the number of slots change.
    const auto midiNotes = getMidiNotes (*getChordNotes (chord));
    const auto numSlots = juce::jmax (beatsPerMeasure * music::getArpeggioNotesPerBeat ((int) midiNotes.size(), beatsPerMeasure),
                                      (int) midiNotes.size());

    const auto orderStillFits = (int) chord.randomOrder.size() == numSlots
        && std::all_of (chord.randomOrder.begin(), chord.randomOrder.end(), [&] (int midi)
                        { return std::find (midiNotes.begin(), midiNotes.end(), midi) != midiNotes.end(); })
        && std::all_of (midiNotes.begin(), midiNotes.end(), [&] (int midi)
                        { return std::find (chord.randomOrder.begin(), chord.randomOrder.end(), midi) != chord.randomOrder.end(); });

    if (reshuffleRandomOrder || ! orderStillFits)
        chord.randomOrder = music::createRandomOrder (midiNotes, numSlots, random);
}
