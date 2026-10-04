#include "Recorder.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    constexpr int middleC = 60;

    /** The written length nearest to a number of 32nds, no longer than the room there is: a
        whole, dotted half, half, dotted quarter, quarter, dotted eighth, eighth or 16th note. Halfway
        between two, it's the shorter.
    */
    int getNearestLength (int slots, int room)
    {
        auto nearest = 0;

        for (auto length : { 2, 4, 6, 8, 12, 16, 24, 32 })
            if (length <= room && (nearest == 0 || std::abs (length - slots) < std::abs (nearest - slots)))
                nearest = length;

        return nearest > 0 ? nearest : Score::fitNoteLength (slots, room);
    }

    double toBeats (int slots) { return (double) slots / Score::slotsPerBeat; }
}

Recorder::Recorder (Score& scoreToUse)
    : score (scoreToUse)
{
}

int Recorder::getScoreSlots() const
{
    return score.getNumMeasures() * score.getBeatsPerMeasure() * Score::slotsPerBeat;
}

void Recorder::keyDown (int midiNote, double beat, int partId)
{
    // A key that's somehow still down is let go of first.
    keyUp (midiNote, beat);

    const auto start = (int) std::lround (beat * Score::slotsPerBeat / grid) * grid;
    const auto staff = midiNote >= middleC ? Staff::treble : Staff::bass;

    if (start >= getScoreSlots())
        return;

    // Notes held on the same staff, from earlier, end where this one starts.
    for (auto& key : held)
        if (key.partId == partId && key.staff == staff && key.start < start)
            key.cut = juce::jmin (key.cut.value_or (start), start);

    held.push_back ({ midiNote, partId, staff, beat, start, {} });
}

void Recorder::keyUp (int midiNote, double beat)
{
    const auto key = std::find_if (held.begin(), held.end(), [midiNote] (const HeldKey& k) { return k.midiNote == midiNote; });

    if (key == held.end())
        return;

    const auto released = *key;
    held.erase (key);
    write (released, beat);
}

void Recorder::finish (std::optional<double> beat)
{
    const auto keys = std::exchange (held, {});

    for (const auto& key : keys)
        write (key, beat.value_or (toBeats (getScoreSlots())));
}

void Recorder::write (const HeldKey& key, double endBeat)
{
    const auto part = score.findPart (key.partId);
    const auto pitch = music::spellMidiNote (score.getKey(), key.midiNote);

    if (part < 0 || pitch.step < 0)
        return;

    const auto slotsPerMeasure = score.getBeatsPerMeasure() * Score::slotsPerBeat;
    const auto scoreSlots = getScoreSlots();

    // A note's at least a 16th, and stops where the next note on its staff starts, or the score ends.
    auto end = endBeat < key.beat ? scoreSlots : (int) std::lround (endBeat * Score::slotsPerBeat / grid) * grid;
    end = juce::jmin (juce::jmax (end, key.start + grid), key.cut.value_or (scoreSlots), scoreSlots);

    // The notes it's written as, by where they start and how long they are, in 32nds from the start of the score
    std::vector<std::pair<int, int>> written;

    for (auto at = key.start; at < end;)
    {
        const auto measure = at / slotsPerMeasure;
        const auto slot = at % slotsPerMeasure;
        const auto barline = (measure + 1) * slotsPerMeasure;
        const auto lastMeasure = end <= barline;
        int length = 0;

        if (! lastMeasure)
        {
            // Up to the barline exactly, to tie across it
            length = Score::fitNoteLength (barline - at, barline - at);
        }
        else
        {
            // The last part of the note can be written a little longer than it was held, but not
            // over the next note on the staff.
            auto room = juce::jmin (barline, key.cut.value_or (barline));

            for (auto later = end; later < room; later += grid)
                if (! score.getNotes (part, key.staff, measure, toBeats (later - measure * slotsPerMeasure)).empty())
                    room = later;

            length = getNearestLength (end - at, room - at);
        }

        // A measure whose chord has the staff is left alone, and nothing's tied across it.
        if (score.chordUsesStaff (part, measure, key.staff))
        {
            at = barline;
            continue;
        }

        score.addNote ({ key.staff, measure, toBeats (slot), pitch, part, toBeats (length) });
        written.push_back ({ at, length });

        // In its last measure, it's the one note, however long it's written.
        if (lastMeasure)
            break;

        at += length;
    }

    for (size_t i = 1; i < written.size(); ++i)
    {
        const auto [from, fromLength] = written[i - 1];
        const auto to = written[i].first;
        const auto fromMeasure = from / slotsPerMeasure, toMeasure = to / slotsPerMeasure;
        const auto fromBeat = toBeats (from % slotsPerMeasure), toBeat = toBeats (to % slotsPerMeasure);

        if (from + fromLength == to && ! score.isTied (part, key.staff, fromMeasure, fromBeat, pitch))
            score.toggleTie (part, key.staff, fromMeasure, fromBeat, toMeasure, toBeat, pitch);
    }
}
