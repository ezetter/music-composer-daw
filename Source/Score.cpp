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

void Score::setAlternateStaff (music::AlternateStaff newAlternateStaff)
{
    if (newAlternateStaff == alternateStaff)
        return;

    alternateStaff = newAlternateStaff;

    // Notes set by hand were for the old alternate staff.
    for (auto& measure : measures)
        if (measure.chord.has_value())
            measure.chord->alternateNotes.reset();

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
        && (chord->style.staff == staff || alternateStaff != music::AlternateStaff::none);
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

    if (! target.chord.has_value() || ! target.chord->hasNotes() || alternateStaff == music::AlternateStaff::none)
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

    if (chord == nullptr || alternateStaff == music::AlternateStaff::none)
        return {};

    if (chord->alternateNotes.has_value())
        return music::createTones (getKey(), *chord->alternateNotes);

    if (const auto notes = getChordNotes (measure))
        return music::getAlternateTones (getKey(), *notes, chord->style.staff, alternateStaff);

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

    // The chord replaces the quarter notes on its staff. The ones on the alternate staff are
    // only hidden, since the alternate staff can be turned off for the whole score at once.
    for (auto& notes : measure.notes[(size_t) chord.style.staff])
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

//==============================================================================
namespace
{
    constexpr auto formatName = "Anthropocene Music score";
    constexpr int formatVersion = 2;    // 2: the alternate staff is the score's, not each chord's

    juce::var notesToJSON (const std::vector<music::KeyboardNote>& notes)
    {
        juce::Array<juce::var> list;

        for (const auto& note : notes)
            list.add (juce::Array<juce::var> { note.midi, note.spelling.letter, note.spelling.alter });

        return list;
    }

    /** A number from the file, or the fallback if it's missing or out of range. */
    int readInt (const juce::var& value, int lowest, int highest, int fallback)
    {
        if (! (value.isInt() || value.isInt64() || value.isDouble()))
            return fallback;

        const auto number = (int) value;
        return number >= lowest && number <= highest ? number : fallback;
    }

    std::optional<std::vector<music::KeyboardNote>> readKeyboardNotes (const juce::var& value)
    {
        const auto* list = value.getArray();

        if (list == nullptr)
            return {};

        std::vector<music::KeyboardNote> notes;

        for (const auto& item : *list)
        {
            const music::KeyboardNote note { readInt (item[0], 0, 127, -1),
                                             { readInt (item[1], 0, 6, -1), readInt (item[2], -2, 2, 99) } };

            // Skip anything that isn't a real note, or whose spelling doesn't match its pitch.
            if (note.midi >= 0 && note.spelling.letter >= 0 && note.spelling.alter != 99
                && note.spelling.getPitchClass() == music::mod (note.midi, 12))
                notes.push_back (note);
        }

        return notes;
    }
}

void Score::clear()
{
    measures = std::vector<Measure> (initialMeasures);
    keyIndex = 0;
    beatsPerMeasure = 4;
    beatsPerMinute = 120.0;
    alternateStaff = music::AlternateStaff::none;
    sendSynchronousChangeMessage();
}

juce::var Score::toJSON() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", formatName);
    root->setProperty ("version", formatVersion);
    root->setProperty ("key", keyIndex);
    root->setProperty ("beatsPerMeasure", beatsPerMeasure);
    root->setProperty ("beatsPerMinute", beatsPerMinute);
    root->setProperty ("alternateStaff", (int) alternateStaff);

    juce::Array<juce::var> measureList;

    for (const auto& measure : measures)
    {
        auto* measureObject = new juce::DynamicObject();

        // Every beat is kept, including ones hidden by a shorter time signature.
        for (auto staff : { Staff::treble, Staff::bass })
        {
            juce::Array<juce::var> beats;

            for (const auto& notes : measure.notes[(size_t) staff])
            {
                juce::Array<juce::var> pitches;

                for (const auto& pitch : notes)
                    pitches.add (juce::Array<juce::var> { pitch.step, pitch.alter });

                beats.add (pitches);
            }

            measureObject->setProperty (staff == Staff::treble ? "treble" : "bass", beats);
        }

        if (measure.chord.has_value())
        {
            const auto& chord = *measure.chord;
            auto* chordObject = new juce::DynamicObject();
            chordObject->setProperty ("degree", chord.spec.degree);
            chordObject->setProperty ("flat", chord.spec.flat);
            chordObject->setProperty ("minor", chord.spec.minor);
            chordObject->setProperty ("altered", chord.spec.altered);
            chordObject->setProperty ("addedNote", (int) chord.spec.addedNote);
            chordObject->setProperty ("inversion", chord.spec.inversion);
            chordObject->setProperty ("octave", chord.spec.octave);
            chordObject->setProperty ("staff", chord.style.staff == Staff::treble ? "treble" : "bass");
            chordObject->setProperty ("type", (int) chord.style.type);

            if (chord.keyboardNotes.has_value())
                chordObject->setProperty ("keyboardNotes", notesToJSON (*chord.keyboardNotes));

            if (chord.alternateNotes.has_value())
                chordObject->setProperty ("alternateNotes", notesToJSON (*chord.alternateNotes));

            juce::Array<juce::var> order;

            for (auto midi : chord.randomOrder)
                order.add (midi);

            chordObject->setProperty ("randomOrder", order);
            measureObject->setProperty ("chord", chordObject);
        }

        measureList.add (measureObject);
    }

    root->setProperty ("measures", measureList);
    return root;
}

juce::Result Score::loadJSON (const juce::var& json)
{
    if (json.getProperty ("format", {}).toString() != formatName)
        return juce::Result::fail ("This isn't an Anthropocene Music score.");

    const auto* measureList = json.getProperty ("measures", {}).getArray();

    if (measureList == nullptr || measureList->isEmpty())
        return juce::Result::fail ("The score has no measures.");

    std::vector<Measure> loaded;
    std::vector<music::AlternateStaff> chordAlternates;     // each chord's own, in scores from before version 2

    for (const auto& item : *measureList)
    {
        Measure measure;

        for (auto staff : { Staff::treble, Staff::bass })
        {
            const auto& beats = item.getProperty (staff == Staff::treble ? "treble" : "bass", {});

            for (int beat = 0; beat < maxBeatsPerMeasure; ++beat)
            {
                auto& notes = measure.notes[(size_t) staff][(size_t) beat];

                if (const auto* pitches = beats[beat].getArray())
                    for (const auto& pitch : *pitches)
                        if (const auto step = readInt (pitch[0], 0, 80, -1); step >= 0)
                            notes.push_back ({ step, readInt (pitch[1], -2, 2, 0) });

                std::sort (notes.begin(), notes.end(), comesBefore);
                notes.erase (std::unique (notes.begin(), notes.end()), notes.end());
            }
        }

        if (const auto& c = item.getProperty ("chord", {}); c.isObject())
        {
            MeasureChord chord;
            chord.spec.degree = readInt (c.getProperty ("degree", {}), -1, 6, -1);
            chord.spec.flat = (bool) c.getProperty ("flat", false);
            chord.spec.minor = (bool) c.getProperty ("minor", false);
            chord.spec.altered = (bool) c.getProperty ("altered", false);
            chord.spec.addedNote = (music::AddedNote) readInt (c.getProperty ("addedNote", {}), 0, 6, 0);
            chord.spec.inversion = readInt (c.getProperty ("inversion", {}), 0, 3, 0);
            chord.spec.octave = readInt (c.getProperty ("octave", {}), -1, 1, 0);
            chord.style.staff = c.getProperty ("staff", {}).toString() == "bass" ? Staff::bass : Staff::treble;
            chord.style.type = (music::ChordType) readInt (c.getProperty ("type", {}), 0, 4, 0);
            chordAlternates.push_back ((music::AlternateStaff) readInt (c.getProperty ("alternate", {}), 0, 4, 0));
            chord.keyboardNotes = readKeyboardNotes (c.getProperty ("keyboardNotes", {}));
            chord.alternateNotes = readKeyboardNotes (c.getProperty ("alternateNotes", {}));

            // Keep only the combinations the chord panel allows: an added note suits the chord's
            // quality and can't go with + / °, and only 7th chords have a 3rd inversion.
            const auto choices = music::getAddedNoteChoices (chord.spec.minor);

            if (chord.spec.addedNote != music::AddedNote::none
                && (chord.spec.altered || std::find (choices.begin(), choices.end(), chord.spec.addedNote) == choices.end()))
                chord.spec.addedNote = music::AddedNote::none;

            if (chord.spec.inversion == 3 && chord.spec.addedNote == music::AddedNote::none)
                chord.spec.inversion = 0;

            if (chord.keyboardNotes.has_value() && chord.keyboardNotes->empty())
                chord.keyboardNotes.reset();

            if (const auto* order = c.getProperty ("randomOrder", {}).getArray())
                for (const auto& midi : *order)
                    chord.randomOrder.push_back (readInt (midi, 0, 127, 0));

            measure.chord = chord;
        }

        loaded.push_back (std::move (measure));
    }

    measures = std::move (loaded);
    keyIndex = readInt (json.getProperty ("key", {}), 0, (int) music::getMajorKeys().size() - 1, 0);
    beatsPerMeasure = readInt (json.getProperty ("beatsPerMeasure", {}), 2, maxBeatsPerMeasure, 4);

    const auto tempo = (double) json.getProperty ("beatsPerMinute", 120.0);
    beatsPerMinute = tempo >= minBeatsPerMinute && tempo <= maxBeatsPerMinute ? tempo : 120.0;

    if (json.hasProperty ("alternateStaff"))
    {
        alternateStaff = (music::AlternateStaff) readInt (json.getProperty ("alternateStaff", {}), 0, 4, 0);
    }
    else
    {
        // Each chord had its own alternate staff, so use the one most of them had. The others'
        // notes set by hand were for a different alternate staff.
        std::array<int, 5> counts {};

        for (auto alternate : chordAlternates)
            ++counts[(size_t) alternate];

        alternateStaff = (music::AlternateStaff) std::distance (counts.begin(), std::max_element (counts.begin(), counts.end()));
        size_t chordIndex = 0;

        for (auto& measure : measures)
            if (measure.chord.has_value())
                if (chordAlternates[chordIndex++] != alternateStaff)
                    measure.chord->alternateNotes.reset();
    }

    // A saved random order is kept if it still fits its chord, and made again if not.
    for (auto& measure : measures)
        tidyChord (measure);

    sendSynchronousChangeMessage();
    return juce::Result::ok();
}
