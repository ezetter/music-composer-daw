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

Score::Measure& Score::getMeasure (int part, int measure)
{
    return parts[(size_t) part].measures[(size_t) measure];
}

const Score::Measure& Score::getMeasure (int part, int measure) const
{
    return parts[(size_t) part].measures[(size_t) measure];
}

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
    for (auto& part : parts)
    {
        for (auto& measure : part.measures)
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
    }

    sendSynchronousChangeMessage();
}

void Score::setBeatsPerMeasure (int newBeatsPerMeasure)
{
    jassert (newBeatsPerMeasure >= 2 && newBeatsPerMeasure <= maxBeatsPerMeasure);

    if (newBeatsPerMeasure == beatsPerMeasure)
        return;

    beatsPerMeasure = newBeatsPerMeasure;

    for (auto& part : parts)
        for (auto& measure : part.measures)
            tidyChord (measure);

    sendSynchronousChangeMessage();
}

void Score::setAlternateStaff (int part, music::AlternateStaff newAlternateStaff)
{
    auto& target = parts[(size_t) part];

    if (newAlternateStaff == target.alternateStaff)
        return;

    target.alternateStaff = newAlternateStaff;

    // Notes set by hand were for the old alternate staff.
    for (auto& measure : target.measures)
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
    for (auto& part : parts)
        part.measures.emplace_back();

    sendSynchronousChangeMessage();
}

void Score::cloneMeasures()
{
    for (auto& part : parts)
    {
        const auto copy = part.measures;
        part.measures.insert (part.measures.end(), copy.begin(), copy.end());
    }

    sendSynchronousChangeMessage();
}

void Score::removeLastMeasure()
{
    if (getNumMeasures() <= 1)
        return;

    for (auto& part : parts)
        part.measures.pop_back();

    pruneTies();
    sendSynchronousChangeMessage();
}

//==============================================================================
bool Score::addNote (const Note& note)
{
    jassert (juce::isPositiveAndBelow (note.measure, getNumMeasures()));
    jassert (juce::isPositiveAndBelow (note.beat, beatsPerMeasure));

    jassert (juce::isPositiveAndBelow (note.part, numParts));

    if (chordUsesStaff (note.part, note.measure, note.staff))
        return false;

    auto& measure = getMeasure (note.part, note.measure);
    auto& staffNotes = measure.notes[(size_t) note.staff];
    auto& lengths = measure.lengths[(size_t) note.staff];
    auto& notes = staffNotes[(size_t) note.beat];
    const auto length = juce::jlimit (1, beatsPerMeasure - note.beat, note.length);
    const auto insertionPoint = std::lower_bound (notes.begin(), notes.end(), note.pitch, comesBefore);
    const auto alreadyThere = insertionPoint != notes.end() && *insertionPoint == note.pitch;

    if (alreadyThere && lengths[(size_t) note.beat] == length)
        return false;

    // A longer note held over this beat is cut short to end here.
    for (int earlier = 0; earlier < note.beat; ++earlier)
        if (! staffNotes[(size_t) earlier].empty() && earlier + lengths[(size_t) earlier] > note.beat)
            lengths[(size_t) earlier] = note.beat - earlier;

    // The new note takes the place of the notes on the beats it covers.
    for (auto covered = note.beat + 1; covered < note.beat + length; ++covered)
    {
        staffNotes[(size_t) covered].clear();
        lengths[(size_t) covered] = 1;
    }

    // Notes already on the beat take the new length, as notes starting together share one.
    lengths[(size_t) note.beat] = length;

    if (! alreadyThere)
        notes.insert (insertionPoint, note.pitch);

    pruneTies();
    sendSynchronousChangeMessage();
    return true;
}

const std::vector<music::Pitch>& Score::getNotes (int part, Staff staff, int measure, int beat) const
{
    return getMeasure (part, measure).notes[(size_t) staff][(size_t) beat];
}

int Score::getNoteLength (int part, Staff staff, int measure, int beat) const
{
    return getMeasure (part, measure).lengths[(size_t) staff][(size_t) beat];
}

//==============================================================================
std::optional<std::pair<int, int>> Score::getFollowingBeat (int part, Staff staff, int measure, int beat) const
{
    // As long as the notes are shown: as added, or as fits in the measure.
    const auto length = juce::jlimit (1, beatsPerMeasure - beat, getNoteLength (part, staff, measure, beat));

    if (beat + length < beatsPerMeasure)
        return std::pair { measure, beat + length };

    if (measure + 1 < getNumMeasures())
        return std::pair { measure + 1, 0 };

    return {};
}

bool Score::isTied (int part, Staff staff, int measure, int beat, music::Pitch pitch) const
{
    if (beat >= beatsPerMeasure)
        return false;

    const auto& ties = getMeasure (part, measure).ties[(size_t) staff][(size_t) beat];

    if (std::find (ties.begin(), ties.end(), pitch) == ties.end())
        return false;

    const auto& notes = getNotes (part, staff, measure, beat);
    const auto next = getFollowingBeat (part, staff, measure, beat);

    if (std::find (notes.begin(), notes.end(), pitch) == notes.end() || ! next.has_value())
        return false;

    const auto& nextNotes = getNotes (part, staff, next->first, next->second);
    return std::find (nextNotes.begin(), nextNotes.end(), pitch) != nextNotes.end();
}

bool Score::toggleTie (int part, Staff staff, int measure, int beat, int otherMeasure, int otherBeat, music::Pitch pitch)
{
    // The tie goes from the earlier note to the later.
    if (std::pair { otherMeasure, otherBeat } < std::pair { measure, beat })
    {
        std::swap (measure, otherMeasure);
        std::swap (beat, otherBeat);
    }

    const auto& notes = getNotes (part, staff, measure, beat);
    const auto& otherNotes = getNotes (part, staff, otherMeasure, otherBeat);

    if (std::find (notes.begin(), notes.end(), pitch) == notes.end()
        || std::find (otherNotes.begin(), otherNotes.end(), pitch) == otherNotes.end()
        || getFollowingBeat (part, staff, measure, beat) != std::optional<std::pair<int, int>> ({ otherMeasure, otherBeat }))
        return false;

    auto& ties = getMeasure (part, measure).ties[(size_t) staff][(size_t) beat];

    if (std::erase (ties, pitch) == 0)
        ties.push_back (pitch);

    sendSynchronousChangeMessage();
    return true;
}

void Score::pruneTies()
{
    for (int part = 0; part < numParts; ++part)
        for (int measure = 0; measure < getNumMeasures(); ++measure)
            for (auto staff : { Staff::treble, Staff::bass })
                for (int beat = 0; beat < maxBeatsPerMeasure; ++beat)
                {
                    // Beats a shorter time signature hides keep their ties, for when they're back.
                    if (beat >= beatsPerMeasure)
                        continue;

                    auto& ties = getMeasure (part, measure).ties[(size_t) staff][(size_t) beat];
                    std::erase_if (ties, [&] (const music::Pitch& pitch) { return ! isTied (part, staff, measure, beat, pitch); });
                }
}

bool Score::hasNoteAt (int part, Staff staff, int measure, int beat, int step) const
{
    const auto& notes = getNotes (part, staff, measure, beat);
    return std::any_of (notes.begin(), notes.end(), [step] (const music::Pitch& pitch) { return pitch.step == step; });
}

bool Score::removeNotesAt (int part, Staff staff, int measure, int beat, int step)
{
    auto& target = getMeasure (part, measure);
    auto& notes = target.notes[(size_t) staff][(size_t) beat];

    if (std::erase_if (notes, [step] (const music::Pitch& pitch) { return pitch.step == step; }) == 0)
        return false;

    if (notes.empty())
        target.lengths[(size_t) staff][(size_t) beat] = 1;

    pruneTies();

    sendSynchronousChangeMessage();
    return true;
}

//==============================================================================
const MeasureChord* Score::getChord (int part, int measure) const
{
    const auto& chord = getMeasure (part, measure).chord;
    return chord.has_value() ? &*chord : nullptr;
}

void Score::setChord (int part, int measure, std::optional<MeasureChord> chord)
{
    auto& target = getMeasure (part, measure);
    target.chord = std::move (chord);
    tidyChord (target);
    pruneTies();
    sendSynchronousChangeMessage();
}

std::optional<music::Chord> Score::getChordNotes (int part, int measure) const
{
    if (const auto* chord = getChord (part, measure))
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

bool Score::chordUsesStaff (int part, int measure, Staff staff) const
{
    const auto* chord = getChord (part, measure);

    return chord != nullptr && chord->hasNotes()
        && (chord->style.staff == staff || getAlternateStaff (part) != music::AlternateStaff::none);
}

void Score::toggleChordNote (int part, int measure, int midiNote, const ChordStyle& styleForNewChord)
{
    auto& target = getMeasure (part, measure);

    if (! target.chord.has_value())
        target.chord = MeasureChord { {}, {}, styleForNewChord, {}, {} };

    toggleChordNote (*target.chord, midiNote);
    tidyChord (target);
    pruneTies();
    sendSynchronousChangeMessage();
}

void Score::toggleChordNote (MeasureChord& chord, int midiNote) const
{
    std::vector<int> midiNotes;

    if (const auto notes = getChordNotes (chord))
        midiNotes = getMidiNotes (*notes);

    if (const auto existing = std::find (midiNotes.begin(), midiNotes.end(), midiNote); existing != midiNotes.end())
        midiNotes.erase (existing);
    else
        midiNotes.push_back (midiNote);

    const auto description = music::describeNotes (getKey(), chord.style.staff, midiNotes);
    chord.spec = description.spec;
    chord.keyboardNotes = description.keyboardNotes;
}

void Score::toggleAlternateNote (int part, int measure, const music::KeyboardNote& note)
{
    auto& target = getMeasure (part, measure);

    if (! target.chord.has_value() || ! target.chord->hasNotes() || getAlternateStaff (part) == music::AlternateStaff::none)
        return;

    auto& chord = *target.chord;

    if (! chord.alternateNotes.has_value())
    {
        // Start from the notes the chord gives the alternate staff.
        std::vector<music::KeyboardNote> notes;

        for (const auto& tone : getAlternateTones (part, measure))
            notes.push_back ({ tone.midi, tone.pitch.getSpelling() });

        chord.alternateNotes = std::move (notes);
    }

    auto& notes = *chord.alternateNotes;

    if (std::erase_if (notes, [&] (const music::KeyboardNote& n) { return n.midi == note.midi; }) == 0)
        notes.push_back (note);

    sendSynchronousChangeMessage();
}

std::vector<music::Tone> Score::getAlternateTones (int part, int measure) const
{
    const auto* chord = getChord (part, measure);
    const auto alternateStaff = getAlternateStaff (part);

    if (chord == nullptr || alternateStaff == music::AlternateStaff::none)
        return {};

    if (chord->alternateNotes.has_value())
        return music::createTones (getKey(), *chord->alternateNotes);

    if (const auto notes = getChordNotes (part, measure))
        return music::getAlternateTones (getKey(), *notes, chord->style.staff, alternateStaff);

    return {};
}

void Score::reshuffle (int part, int measure)
{
    tidyChord (getMeasure (part, measure), true);
    sendSynchronousChangeMessage();
}

//==============================================================================
bool Score::hasNotes (int part) const
{
    for (const auto& measure : parts[(size_t) part].measures)
        for (const auto& staff : measure.notes)
            for (const auto& notes : staff)
                if (! notes.empty())
                    return true;

    return hasChords (part);
}

bool Score::hasChords (int part) const
{
    const auto& measures = parts[(size_t) part].measures;
    return std::any_of (measures.begin(), measures.end(), [] (const Measure& m) { return m.chord.has_value() && m.chord->hasNotes(); });
}

void Score::copyChords (int fromPart, int toPart)
{
    jassert (fromPart != toPart);

    const auto& from = parts[(size_t) fromPart];
    auto& to = parts[(size_t) toPart];

    for (size_t m = 0; m < to.measures.size(); ++m)
    {
        auto& measure = to.measures[m];
        measure.clearNotes (Staff::treble);
        measure.clearNotes (Staff::bass);
        measure.chord = from.measures[m].chord;

        // Notes set by hand were for the other part's alternate staff.
        if (measure.chord.has_value() && from.alternateStaff != to.alternateStaff)
            measure.chord->alternateNotes.reset();

        tidyChord (measure);
    }

    pruneTies();
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
    // only hidden, since the alternate staff can be turned off for a whole part at once.
    measure.clearNotes (chord.style.staff);

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
    // 2: the alternate staff is the score's, not each chord's
    // 3: two parts, each with its own measures and alternate staff
    constexpr int formatVersion = 3;

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
    parts = {};
    keyIndex = 0;
    beatsPerMeasure = 4;
    beatsPerMinute = 120.0;
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

    const auto measureToJSON = [] (const Measure& measure)
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

            // How many beats the notes on each beat last
            juce::Array<juce::var> lengths;

            for (auto length : measure.lengths[(size_t) staff])
                lengths.add (length);

            measureObject->setProperty (staff == Staff::treble ? "trebleLengths" : "bassLengths", lengths);

            // The notes on each beat that are tied to the next
            juce::Array<juce::var> ties;

            for (const auto& tied : measure.ties[(size_t) staff])
            {
                juce::Array<juce::var> pitches;

                for (const auto& pitch : tied)
                    pitches.add (juce::Array<juce::var> { pitch.step, pitch.alter });

                ties.add (pitches);
            }

            measureObject->setProperty (staff == Staff::treble ? "trebleTies" : "bassTies", ties);
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

        return juce::var (measureObject);
    };

    juce::Array<juce::var> partList;

    for (const auto& part : parts)
    {
        juce::Array<juce::var> measureList;

        for (const auto& measure : part.measures)
            measureList.add (measureToJSON (measure));

        auto* partObject = new juce::DynamicObject();
        partObject->setProperty ("alternateStaff", (int) part.alternateStaff);
        partObject->setProperty ("measures", measureList);
        partList.add (partObject);
    }

    root->setProperty ("parts", partList);
    return root;
}

juce::Result Score::loadJSON (const juce::var& json)
{
    if (json.getProperty ("format", {}).toString() != formatName)
        return juce::Result::fail ("This isn't an Anthropocene Music score.");

    // Scores from before version 3 have one part, with its measures and alternate staff at the top.
    juce::Array<juce::var> partList;

    if (const auto* list = json.getProperty ("parts", {}).getArray())
        partList = *list;
    else
        partList.add (json);

    std::array<Part, numParts> loadedParts;
    auto numMeasures = 0;

    for (int partIndex = 0; partIndex < numParts && partIndex < partList.size(); ++partIndex)
    {
        const auto& partJSON = partList.getReference (partIndex);
        const auto* measureList = partJSON.getProperty ("measures", {}).getArray();

        if (measureList == nullptr)
            continue;

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

                    // Scores from before notes had lengths have quarter notes.
                    const auto* lengths = item.getProperty (staff == Staff::treble ? "trebleLengths" : "bassLengths", {}).getArray();
                    const auto hasLength = lengths != nullptr && beat < lengths->size();
                    measure.lengths[(size_t) staff][(size_t) beat] = notes.empty() || ! hasLength ? 1 : readInt (lengths->getReference (beat), 1, maxNoteLength, 1);

                    // Ties, from notes that are there. Ones that don't lead anywhere are let go
                    // once the whole score's loaded.
                    if (const auto* ties = item.getProperty (staff == Staff::treble ? "trebleTies" : "bassTies", {}).getArray(); ties != nullptr && beat < ties->size())
                        if (const auto* pitches = ties->getReference (beat).getArray())
                            for (const auto& pitch : *pitches)
                                if (const music::Pitch tied { readInt (pitch[0], 0, 80, -1), readInt (pitch[1], -2, 2, 0) };
                                    std::find (notes.begin(), notes.end(), tied) != notes.end())
                                    measure.ties[(size_t) staff][(size_t) beat].push_back (tied);
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

        auto& part = loadedParts[(size_t) partIndex];
        part.measures = std::move (loaded);
        numMeasures = juce::jmax (numMeasures, (int) part.measures.size());

        if (partJSON.hasProperty ("alternateStaff"))
        {
            part.alternateStaff = (music::AlternateStaff) readInt (partJSON.getProperty ("alternateStaff", {}), 0, 4, 0);
        }
        else
        {
            // Each chord had its own alternate staff, so use the one most of them had. The others'
            // notes set by hand were for a different alternate staff.
            std::array<int, 5> counts {};

            for (auto alternate : chordAlternates)
                ++counts[(size_t) alternate];

            part.alternateStaff = (music::AlternateStaff) std::distance (counts.begin(), std::max_element (counts.begin(), counts.end()));
            size_t chordIndex = 0;

            for (auto& measure : part.measures)
                if (measure.chord.has_value())
                    if (chordAlternates[chordIndex++] != part.alternateStaff)
                        measure.chord->alternateNotes.reset();
        }
    }

    if (numMeasures == 0)
        return juce::Result::fail ("The score has no measures.");

    // Every part has as many measures as the longest.
    for (auto& part : loadedParts)
        part.measures.resize ((size_t) numMeasures);

    parts = std::move (loadedParts);
    keyIndex = readInt (json.getProperty ("key", {}), 0, (int) music::getMajorKeys().size() - 1, 0);
    beatsPerMeasure = readInt (json.getProperty ("beatsPerMeasure", {}), 2, maxBeatsPerMeasure, 4);

    const auto tempo = (double) json.getProperty ("beatsPerMinute", 120.0);
    beatsPerMinute = tempo >= minBeatsPerMinute && tempo <= maxBeatsPerMinute ? tempo : 120.0;

    // A saved random order is kept if it still fits its chord, and made again if not.
    for (auto& part : parts)
        for (auto& measure : part.measures)
            tidyChord (measure);

    pruneTies();
    sendSynchronousChangeMessage();
    return juce::Result::ok();
}
