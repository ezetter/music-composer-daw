#include "Score.h"

#include <algorithm>
#include <limits>

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

Score::Score()
{
    for (int part = 0; part < initialParts; ++part)
        parts.push_back ({ nextPartId++ });
}

//==============================================================================
int Score::addPart()
{
    Part part { nextPartId++ };
    part.measures.resize ((size_t) getNumMeasures());
    parts.push_back (std::move (part));
    sendSynchronousChangeMessage();
    return getNumParts() - 1;
}

void Score::removePart (int part)
{
    if (getNumParts() <= 1 || ! juce::isPositiveAndBelow (part, getNumParts()))
        return;

    parts.erase (parts.begin() + part);
    sendSynchronousChangeMessage();
}

int Score::findPart (int id) const noexcept
{
    for (size_t part = 0; part < parts.size(); ++part)
        if (parts[part].id == id)
            return (int) part;

    return -1;
}

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
    const auto slot = toSlot (note.beat);
    const auto slots = getSlotsPerMeasure();

    jassert (juce::isPositiveAndBelow (note.measure, getNumMeasures()));
    jassert (juce::isPositiveAndBelow (slot, slots));
    jassert (juce::isPositiveAndBelow (note.part, getNumParts()));

    if (chordUsesStaff (note.part, note.measure, note.staff))
        return false;

    auto& measure = getMeasure (note.part, note.measure);
    auto& staffNotes = measure.notes[(size_t) note.staff];
    auto& lengths = measure.lengths[(size_t) note.staff];
    auto& notes = staffNotes[(size_t) slot];
    const auto length = fitNoteLength (toSlot (note.length), slots - slot);
    const auto insertionPoint = std::lower_bound (notes.begin(), notes.end(), note.pitch, comesBefore);
    const auto alreadyThere = insertionPoint != notes.end() && *insertionPoint == note.pitch;

    if (alreadyThere && lengths[(size_t) slot] == length)
        return false;

    // A longer note held over this point is cut short to end here, as long as a note can be.
    for (int earlier = 0; earlier < slot; ++earlier)
        if (! staffNotes[(size_t) earlier].empty() && earlier + lengths[(size_t) earlier] > slot)
            lengths[(size_t) earlier] = fitNoteLength (slot - earlier, slot - earlier);

    // The new note takes the place of the notes it covers.
    for (auto covered = slot + 1; covered < slot + length; ++covered)
    {
        staffNotes[(size_t) covered].clear();
        lengths[(size_t) covered] = slotsPerBeat;
    }

    // Notes already there take the new length, as notes starting together share one.
    lengths[(size_t) slot] = length;

    if (! alreadyThere)
        notes.insert (insertionPoint, note.pitch);

    pruneTies();
    sendSynchronousChangeMessage();
    return true;
}

int Score::fitNoteLength (int slots, int room)
{
    // Whole, dotted half, half, dotted quarter, quarter, dotted eighth, eighth, 16th and 32nd notes, in 32nds
    for (auto length : { 32, 24, 16, 12, 8, 6, 4, 2, 1 })
        if (length <= slots && length <= room)
            return length;

    return 1;
}

const std::vector<music::Pitch>& Score::getNotes (int part, Staff staff, int measure, double beat) const
{
    return getMeasure (part, measure).notes[(size_t) staff][(size_t) toSlot (beat)];
}

double Score::getNoteLength (int part, Staff staff, int measure, double beat) const
{
    return toBeats (getMeasure (part, measure).lengths[(size_t) staff][(size_t) toSlot (beat)]);
}

//==============================================================================
std::optional<std::pair<int, int>> Score::getFollowingSlot (int part, Staff staff, int measure, int slot) const
{
    // As long as the notes are shown: as added, or as fits in the measure.
    const auto slots = getSlotsPerMeasure();
    const auto length = fitNoteLength (getMeasure (part, measure).lengths[(size_t) staff][(size_t) slot], slots - slot);

    if (slot + length < slots)
        return std::pair { measure, slot + length };

    if (measure + 1 < getNumMeasures())
        return std::pair { measure + 1, 0 };

    return {};
}

std::optional<std::pair<int, double>> Score::getFollowingBeat (int part, Staff staff, int measure, double beat) const
{
    if (const auto next = getFollowingSlot (part, staff, measure, toSlot (beat)))
        return std::pair { next->first, toBeats (next->second) };

    return {};
}

bool Score::isTied (int part, Staff staff, int measure, double beat, music::Pitch pitch) const
{
    const auto slot = toSlot (beat);

    if (slot >= getSlotsPerMeasure())
        return false;

    const auto& ties = getMeasure (part, measure).ties[(size_t) staff][(size_t) slot];

    if (std::find (ties.begin(), ties.end(), pitch) == ties.end())
        return false;

    const auto& notes = getNotes (part, staff, measure, beat);
    const auto next = getFollowingSlot (part, staff, measure, slot);

    if (std::find (notes.begin(), notes.end(), pitch) == notes.end() || ! next.has_value())
        return false;

    const auto& nextNotes = getMeasure (part, next->first).notes[(size_t) staff][(size_t) next->second];
    return std::find (nextNotes.begin(), nextNotes.end(), pitch) != nextNotes.end();
}

bool Score::toggleTie (int part, Staff staff, int measure, double beat, int otherMeasure, double otherBeat, music::Pitch pitch)
{
    auto slot = toSlot (beat), otherSlot = toSlot (otherBeat);

    // The tie goes from the earlier note to the later.
    if (std::pair { otherMeasure, otherSlot } < std::pair { measure, slot })
    {
        std::swap (measure, otherMeasure);
        std::swap (slot, otherSlot);
    }

    const auto& notes = getMeasure (part, measure).notes[(size_t) staff][(size_t) slot];
    const auto& otherNotes = getMeasure (part, otherMeasure).notes[(size_t) staff][(size_t) otherSlot];

    if (std::find (notes.begin(), notes.end(), pitch) == notes.end()
        || std::find (otherNotes.begin(), otherNotes.end(), pitch) == otherNotes.end()
        || getFollowingSlot (part, staff, measure, slot) != std::optional<std::pair<int, int>> ({ otherMeasure, otherSlot }))
        return false;

    auto& ties = getMeasure (part, measure).ties[(size_t) staff][(size_t) slot];

    if (std::erase (ties, pitch) == 0)
        ties.push_back (pitch);

    sendSynchronousChangeMessage();
    return true;
}

void Score::pruneTies()
{
    // Notes a shorter time signature hides keep their ties, for when they're back.
    for (int part = 0; part < getNumParts(); ++part)
        for (int measure = 0; measure < getNumMeasures(); ++measure)
            for (auto staff : { Staff::treble, Staff::bass })
                for (int slot = 0; slot < getSlotsPerMeasure(); ++slot)
                {
                    auto& ties = getMeasure (part, measure).ties[(size_t) staff][(size_t) slot];
                    std::erase_if (ties, [&] (const music::Pitch& pitch) { return ! isTied (part, staff, measure, toBeats (slot), pitch); });
                }
}

bool Score::hasNoteAt (int part, Staff staff, int measure, double beat, int step) const
{
    const auto& notes = getNotes (part, staff, measure, beat);
    return std::any_of (notes.begin(), notes.end(), [step] (const music::Pitch& pitch) { return pitch.step == step; });
}

bool Score::removeNotesAt (int part, Staff staff, int measure, double beat, int step)
{
    const auto slot = (size_t) toSlot (beat);
    auto& target = getMeasure (part, measure);
    auto& notes = target.notes[(size_t) staff][slot];

    if (std::erase_if (notes, [step] (const music::Pitch& pitch) { return pitch.step == step; }) == 0)
        return false;

    if (notes.empty())
        target.lengths[(size_t) staff][slot] = slotsPerBeat;

    pruneTies();

    sendSynchronousChangeMessage();
    return true;
}

//==============================================================================
std::optional<music::Dynamic> Score::getDynamic (int part, int measure, double beat) const
{
    return getMeasure (part, measure).dynamics[(size_t) toSlot (beat)];
}

void Score::setDynamic (int part, int measure, double beat, std::optional<music::Dynamic> dynamic)
{
    jassert (juce::isPositiveAndBelow (toSlot (beat), maxSlotsPerMeasure));
    auto& marked = getMeasure (part, measure).dynamics[(size_t) toSlot (beat)];

    if (marked == dynamic)
        return;

    marked = dynamic;
    sendSynchronousChangeMessage();
}

std::optional<music::Dynamic> Score::getDynamicInForce (int part, int measure, double beat) const
{
    for (auto slot = juce::jmin (toSlot (beat), getSlotsPerMeasure() - 1); measure >= 0; slot = getSlotsPerMeasure() - 1, --measure)
        for (; slot >= 0; --slot)
            if (const auto dynamic = getMeasure (part, measure).dynamics[(size_t) slot])
                return dynamic;

    return {};
}

std::vector<HairpinMark> Score::getHairpins (int part) const
{
    const auto slotsPerMeasure = getSlotsPerMeasure();
    const auto scoreEnd = getNumMeasures() * slotsPerMeasure;
    std::vector<HairpinMark> hairpins;

    for (int measure = 0; measure < getNumMeasures(); ++measure)
        for (int slot = 0; slot < slotsPerMeasure; ++slot)
            if (const auto& hairpin = getMeasure (part, measure).hairpins[(size_t) slot])
                hairpins.push_back ({ measure, toBeats (slot), hairpin->type,
                                      toBeats (juce::jmin (hairpin->length, scoreEnd - measure * slotsPerMeasure - slot)) });

    return hairpins;
}

void Score::setHairpin (int part, const HairpinMark& hairpin)
{
    const auto slotsPerMeasure = getSlotsPerMeasure();
    const auto slot = toSlot (hairpin.beat);

    if (! juce::isPositiveAndBelow (hairpin.measure, getNumMeasures()) || ! juce::isPositiveAndBelow (slot, slotsPerMeasure))
        return;

    // In 32nds from the start of the score, at least an eighth note long if there's room
    const auto start = hairpin.measure * slotsPerMeasure + slot;
    const auto room = getNumMeasures() * slotsPerMeasure - start;
    const auto end = start + juce::jlimit (juce::jmin (slotsPerBeat / 2, room), room, toSlot (hairpin.length));

    // The hairpins it overlaps make way for it.
    for (const auto& other : getHairpins (part))
    {
        const auto otherStart = other.measure * slotsPerMeasure + toSlot (other.beat);

        if (otherStart < end && otherStart + toSlot (other.length) > start)
            getMeasure (part, other.measure).hairpins[(size_t) toSlot (other.beat)].reset();
    }

    getMeasure (part, hairpin.measure).hairpins[(size_t) slot] = HairpinStart { hairpin.type, end - start };
    sendSynchronousChangeMessage();
}

void Score::removeHairpin (int part, int measure, double beat)
{
    auto& hairpin = getMeasure (part, measure).hairpins[(size_t) toSlot (beat)];

    if (! hairpin.has_value())
        return;

    hairpin.reset();
    sendSynchronousChangeMessage();
}

std::vector<PedalSpan> Score::getPedals (int part) const
{
    // Walking through the marks in order: down starts one, up ends it, and another down without
    // an up in between ends it too.
    const auto slotsPerMeasure = getSlotsPerMeasure();
    std::vector<PedalSpan> spans;
    auto down = false;

    for (int measure = 0; measure < getNumMeasures(); ++measure)
    {
        for (int slot = 0; slot < slotsPerMeasure; ++slot)
        {
            const auto marks = getMeasure (part, measure).pedalMarks[(size_t) slot];
            const auto at = toBeats (measure * slotsPerMeasure + slot);

            if ((marks & pedalUp) != 0 && down)
            {
                spans.back().end = at;
                down = false;
            }

            if ((marks & pedalDown) != 0)
            {
                if (down)
                {
                    spans.back().end = at;
                    spans.back().lifted = false;
                }

                spans.push_back ({ at, at, true });
                down = true;
            }
        }
    }

    if (down)
    {
        spans.back().end = toBeats (getNumMeasures() * slotsPerMeasure);
        spans.back().lifted = false;
    }

    return spans;
}

void Score::setPedals (int part, const std::vector<PedalSpan>& spans)
{
    const auto slotsPerMeasure = getSlotsPerMeasure();
    const auto scoreEnd = getNumMeasures() * slotsPerMeasure;

    for (auto& measure : parts[(size_t) part].measures)
        std::fill (measure.pedalMarks.begin(), measure.pedalMarks.begin() + slotsPerMeasure, (juce::uint8) 0);

    const auto mark = [&] (double beats, juce::uint8 what)
    {
        if (const auto at = toSlot (beats); juce::isPositiveAndBelow (at, scoreEnd))
            getMeasure (part, at / slotsPerMeasure).pedalMarks[(size_t) (at % slotsPerMeasure)] |= what;
    };

    for (const auto& span : spans)
    {
        mark (span.start, pedalDown);

        if (span.lifted)
            mark (span.end, pedalUp);
    }

    sendSynchronousChangeMessage();
}

int Score::getVelocity (int part, int measure, double beat) const
{
    // Everything's counted in 32nds from the start of the score, up to the point asked about.
    const auto slotsPerMeasure = getSlotsPerMeasure();
    const auto point = measure * slotsPerMeasure + juce::jmin (toSlot (beat), slotsPerMeasure);
    auto velocity = (double) music::unmarkedVelocity;
    auto at = 0;
    std::optional<std::pair<music::Hairpin, int>> hairpin;      // the one under way, and where it ends

    const auto moveOn = [&] (int to)
    {
        if (hairpin.has_value())
        {
            if (const auto until = juce::jmin (to, hairpin->second); until > at)
                velocity = music::applyHairpin (hairpin->first, velocity, toBeats (until - at));

            if (to >= hairpin->second)
                hairpin.reset();
        }

        at = to;
    };

    for (int m = 0; m <= measure && m < getNumMeasures(); ++m)
    {
        for (int slot = 0; slot < slotsPerMeasure && m * slotsPerMeasure + slot <= point; ++slot)
        {
            const auto position = m * slotsPerMeasure + slot;
            const auto& marks = getMeasure (part, m);
            moveOn (position);

            // A dynamic sets the velocity, and a hairpin moves it on from there.
            if (const auto& dynamic = marks.dynamics[(size_t) slot])
                velocity = music::getDynamicVelocity (*dynamic);

            if (const auto& start = marks.hairpins[(size_t) slot])
                hairpin = std::pair { start->type, position + start->length };
        }
    }

    moveOn (point);
    return juce::jlimit (1, 127, (int) std::lround (velocity));
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

void Score::replaceChordWithNotes (int part, int measure, const std::vector<Note>& notes)
{
    auto& target = getMeasure (part, measure);
    target.chord.reset();
    target.clearNotes (Staff::treble);
    target.clearNotes (Staff::bass);

    for (const auto& note : notes)
    {
        const auto slot = toSlot (note.beat);

        if (! juce::isPositiveAndBelow (slot, getSlotsPerMeasure()))
            continue;

        auto& pitches = target.notes[(size_t) note.staff][(size_t) slot];

        if (std::find (pitches.begin(), pitches.end(), note.pitch) == pitches.end())
            pitches.insert (std::lower_bound (pitches.begin(), pitches.end(), note.pitch, comesBefore), note.pitch);

        target.lengths[(size_t) note.staff][(size_t) slot] = fitNoteLength (toSlot (note.length), getSlotsPerMeasure() - slot);
    }

    pruneTies();
    sendSynchronousChangeMessage();
}

int Score::getChordNoteLength (const MeasureChord& chord) const
{
    const auto measureLength = getSlotsPerMeasure();

    if (chord.style.noteLength.has_value())
        return fitNoteLength (*chord.style.noteLength, measureLength);

    if (! music::isMelodic (chord.style.type))
        return measureLength;

    const auto notes = getChordNotes (chord);
    const auto numNotes = notes.has_value() ? (int) notes->tones.size() : 1;
    return slotsPerBeat / music::getArpeggioNotesPerBeat (numNotes, beatsPerMeasure);
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

bool Score::isMeasureEmpty (int measure) const
{
    for (int part = 0; part < getNumParts(); ++part)
    {
        const auto& target = getMeasure (part, measure);

        if (target.chord.has_value() && target.chord->hasNotes())
            return false;

        for (const auto& staff : target.notes)
            for (int slot = 0; slot < getSlotsPerMeasure(); ++slot)
                if (! staff[(size_t) slot].empty())
                    return false;
    }

    return true;
}

bool Score::isPartEmpty (int part) const
{
    if (hasNotes (part) || ! getHairpins (part).empty() || ! getPedals (part).empty())
        return false;

    for (const auto& measure : parts[(size_t) part].measures)
        for (const auto& dynamic : measure.dynamics)
            if (dynamic.has_value())
                return false;

    return true;
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

    // A Random chord has a note for each of its note lengths that fits in the measure, and keeps
    // its order until its notes or their number change. Every note is in it, if there's room.
    const auto midiNotes = getMidiNotes (*getChordNotes (chord));
    const auto numSlots = juce::jmax (1, getSlotsPerMeasure() / getChordNoteLength (chord));

    const auto orderStillFits = (int) chord.randomOrder.size() == numSlots
        && std::all_of (chord.randomOrder.begin(), chord.randomOrder.end(), [&] (int midi)
                        { return std::find (midiNotes.begin(), midiNotes.end(), midi) != midiNotes.end(); })
        && (numSlots < (int) midiNotes.size()
            || std::all_of (midiNotes.begin(), midiNotes.end(), [&] (int midi)
                            { return std::find (chord.randomOrder.begin(), chord.randomOrder.end(), midi) != chord.randomOrder.end(); }));

    if (reshuffleRandomOrder || ! orderStillFits)
        chord.randomOrder = music::createRandomOrder (midiNotes, numSlots, random);
}

//==============================================================================
namespace
{
    constexpr auto formatName = "Anthropocene Music score";
    // 2: the alternate staff is the score's, not each chord's
    // 3: two parts, each with its own measures and alternate staff
    // 4: notes by eighth note, rather than by beat, with their lengths in eighths
    // 5: dynamics
    // 6: crescendos and decrescendos
    // 7: any number of parts
    // 8: notes, dynamics and hairpins by 32nd note, rather than by eighth
    // 9: a chord's note length
    // 10: the sustain pedal
    constexpr int formatVersion = 10;

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
    std::vector<Part> emptied;

    for (int part = 0; part < initialParts; ++part)
        emptied.push_back ({ part < getNumParts() ? parts[(size_t) part].id : nextPartId++ });

    parts = std::move (emptied);
    keyIndex = 0;
    beatsPerMeasure = 4;
    beatsPerMinute = 120.0;
    sendSynchronousChangeMessage();
}

juce::var Score::toJSON (bool withPartIds) const
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

        // By 32nd note, every one kept, including ones hidden by a shorter time signature
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

            // How many 32nds the notes starting at each 32nd last
            juce::Array<juce::var> lengths;

            for (auto length : measure.lengths[(size_t) staff])
                lengths.add (length);

            measureObject->setProperty (staff == Staff::treble ? "trebleLengths" : "bassLengths", lengths);

            // The notes starting at each 32nd that are tied to the next
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

        // The dynamic marked at each 32nd, or "" for none, if there are any
        if (std::any_of (measure.dynamics.begin(), measure.dynamics.end(), [] (const auto& d) { return d.has_value(); }))
        {
            juce::Array<juce::var> dynamics;

            for (const auto& dynamic : measure.dynamics)
                dynamics.add (dynamic.has_value() ? music::getDynamicMark (*dynamic) : juce::String());

            measureObject->setProperty ("dynamics", dynamics);
        }

        // The hairpins starting in the measure: at which 32nd, which, and how many 32nds long
        juce::Array<juce::var> hairpins;

        for (int slot = 0; slot < maxSlotsPerMeasure; ++slot)
        {
            if (const auto& hairpin = measure.hairpins[(size_t) slot])
            {
                auto* hairpinObject = new juce::DynamicObject();
                hairpinObject->setProperty ("at", slot);
                hairpinObject->setProperty ("type", hairpin->type == music::Hairpin::crescendo ? "crescendo" : "decrescendo");
                hairpinObject->setProperty ("length", hairpin->length);
                hairpins.add (hairpinObject);
            }
        }

        if (! hairpins.isEmpty())
            measureObject->setProperty ("hairpins", hairpins);

        // The pedal marks: at which 32nd, and whether the pedal comes up, goes down, or both
        juce::Array<juce::var> pedals;

        for (int slot = 0; slot < maxSlotsPerMeasure; ++slot)
        {
            if (const auto marks = measure.pedalMarks[(size_t) slot]; marks != 0)
            {
                auto* pedalObject = new juce::DynamicObject();
                pedalObject->setProperty ("at", slot);
                pedalObject->setProperty ("up", (marks & pedalUp) != 0);
                pedalObject->setProperty ("down", (marks & pedalDown) != 0);
                pedals.add (pedalObject);
            }
        }

        if (! pedals.isEmpty())
            measureObject->setProperty ("pedals", pedals);

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

            if (chord.style.noteLength.has_value())
                chordObject->setProperty ("noteLength", *chord.style.noteLength);

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

        if (withPartIds)
            partObject->setProperty ("id", part.id);

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

    // Scores from before version 3 had a second, empty part all the same.
    std::vector<Part> loadedParts ((size_t) juce::jmax (partList.size(), json.hasProperty ("parts") ? 1 : initialParts));
    auto numMeasures = 0;

    for (int partIndex = 0; partIndex < partList.size(); ++partIndex)
    {
        const auto& partJSON = partList.getReference (partIndex);
        const auto* measureList = partJSON.getProperty ("measures", {}).getArray();

        if (measureList == nullptr)
            continue;

        std::vector<Measure> loaded;
        std::vector<music::AlternateStaff> chordAlternates;     // each chord's own, in scores from before version 2

        // Scores from before version 8 have their notes, dynamics and hairpins by eighth note, and
        // before version 4, by beat, with their lengths the same way.
        const auto version = readInt (json.getProperty ("version", {}), 1, 1000, 1);
        const auto slotsPerEntry = version >= 8 ? 1 : version >= 4 ? slotsPerBeat / 2 : slotsPerBeat;

        for (const auto& item : *measureList)
        {
            Measure measure;

            for (auto staff : { Staff::treble, Staff::bass })
            {
                const auto& entries = item.getProperty (staff == Staff::treble ? "treble" : "bass", {});
                const auto* lengths = item.getProperty (staff == Staff::treble ? "trebleLengths" : "bassLengths", {}).getArray();
                const auto* ties = item.getProperty (staff == Staff::treble ? "trebleTies" : "bassTies", {}).getArray();

                for (int entry = 0; entry < maxSlotsPerMeasure / slotsPerEntry; ++entry)
                {
                    const auto slot = (size_t) (entry * slotsPerEntry);
                    auto& notes = measure.notes[(size_t) staff][slot];

                    if (const auto* pitches = entries[entry].getArray())
                        for (const auto& pitch : *pitches)
                            if (const auto step = readInt (pitch[0], 0, 80, -1); step >= 0)
                                notes.push_back ({ step, readInt (pitch[1], -2, 2, 0) });

                    std::sort (notes.begin(), notes.end(), comesBefore);
                    notes.erase (std::unique (notes.begin(), notes.end()), notes.end());

                    // Scores from before notes had lengths have quarter notes.
                    const auto hasLength = lengths != nullptr && entry < lengths->size();
                    const auto length = hasLength ? readInt (lengths->getReference (entry), 1, maxSlotsPerMeasure / slotsPerEntry, 0) * slotsPerEntry : 0;
                    measure.lengths[(size_t) staff][slot] = notes.empty() || length == 0 ? slotsPerBeat : fitNoteLength (length, maxSlotsPerMeasure - (int) slot);

                    // Ties, from notes that are there. Ones that don't lead anywhere are let go
                    // once the whole score's loaded.
                    if (ties != nullptr && entry < ties->size())
                        if (const auto* pitches = ties->getReference (entry).getArray())
                            for (const auto& pitch : *pitches)
                                if (const music::Pitch tied { readInt (pitch[0], 0, 80, -1), readInt (pitch[1], -2, 2, 0) };
                                    std::find (notes.begin(), notes.end(), tied) != notes.end())
                                    measure.ties[(size_t) staff][slot].push_back (tied);
                }
            }

            if (const auto* dynamics = item.getProperty ("dynamics", {}).getArray())
                for (int entry = 0; entry < maxSlotsPerMeasure / slotsPerEntry && entry < dynamics->size(); ++entry)
                    measure.dynamics[(size_t) (entry * slotsPerEntry)] = music::findDynamic (dynamics->getReference (entry).toString());

            if (const auto* pedals = item.getProperty ("pedals", {}).getArray())
                for (const auto& pedal : *pedals)
                    if (const auto slot = readInt (pedal.getProperty ("at", {}), 0, maxSlotsPerMeasure - 1, -1); slot >= 0)
                        measure.pedalMarks[(size_t) slot] = (juce::uint8) (((bool) pedal.getProperty ("up", false) ? pedalUp : 0)
                                                                           | ((bool) pedal.getProperty ("down", false) ? pedalDown : 0));

            if (const auto* hairpins = item.getProperty ("hairpins", {}).getArray())
            {
                for (const auto& hairpin : *hairpins)
                {
                    // By eighth note, under other names, before version 8
                    const auto entry = readInt (hairpin.getProperty (version >= 8 ? "at" : "eighth", {}), 0, maxSlotsPerMeasure / slotsPerEntry - 1, -1);
                    const auto slot = entry * slotsPerEntry;
                    const auto length = readInt (hairpin.getProperty (version >= 8 ? "length" : "eighths", {}), 1, std::numeric_limits<int>::max() / slotsPerEntry, 0) * slotsPerEntry;
                    const auto type = hairpin.getProperty ("type", {}).toString();

                    if (slot >= 0 && length > 0 && (type == "crescendo" || type == "decrescendo"))
                        measure.hairpins[(size_t) slot] = HairpinStart { type == "crescendo" ? music::Hairpin::crescendo : music::Hairpin::decrescendo, length };
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

                // One of the note lengths there are, in 32nds
                if (const auto length = readInt (c.getProperty ("noteLength", {}), 1, maxSlotsPerMeasure, 0); length > 0)
                    chord.style.noteLength = fitNoteLength (length, length);

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

    // Each part gets the id it was saved with, or the one the part at its place has, or a new one.
    std::vector<int> ids;

    for (size_t part = 0; part < loadedParts.size(); ++part)
    {
        const auto saved = partList.size() > (int) part ? readInt (partList.getReference ((int) part).getProperty ("id", {}), 1, std::numeric_limits<int>::max(), 0) : 0;
        const auto here = part < parts.size() ? parts[part].id : 0;
        const auto unused = [&ids] (int id) { return id > 0 && std::find (ids.begin(), ids.end(), id) == ids.end(); };
        ids.push_back (unused (saved) ? saved : unused (here) ? here : 0);
    }

    nextPartId = juce::jmax (nextPartId, *std::max_element (ids.begin(), ids.end()) + 1);

    for (size_t part = 0; part < loadedParts.size(); ++part)
        loadedParts[part].id = ids[part] > 0 ? ids[part] : nextPartId++;

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
