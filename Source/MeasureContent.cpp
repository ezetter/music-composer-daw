#include "MeasureContent.h"

#include <algorithm>

namespace
{
    music::Tone makeTone (const music::Pitch& pitch, const std::array<int, 7>& keyAlterations)
    {
        return { pitch, pitch.getMidiNoteNumber(), pitch.alter == keyAlterations[(size_t) pitch.getLetter()] };
    }

    /** The chord's notes in the order its chord type writes them. */
    std::vector<music::Tone> getSequence (const music::Chord& chord, const MeasureChord& measureChord)
    {
        auto tones = chord.tones;

        switch (measureChord.style.type)
        {
            case music::ChordType::arpeggioDown:
                std::reverse (tones.begin(), tones.end());
                return tones;

            case music::ChordType::random:
            {
                std::vector<music::Tone> sequence;

                for (auto midi : measureChord.randomOrder)
                    if (const auto tone = std::find_if (tones.begin(), tones.end(), [midi] (const music::Tone& t) { return t.midi == midi; });
                        tone != tones.end())
                        sequence.push_back (*tone);

                return sequence.empty() ? tones : sequence;
            }

            case music::ChordType::arpeggioUp:
            case music::ChordType::block:
            case music::ChordType::rolled:
                break;
        }

        return tones;
    }

    /** Rests filling an empty stretch of a measure, from one 32nd note to another. A whole beat
        gets a quarter rest, and two that make up the first half of 4/4 or 3/4, or the second half
        of 4/4, share a half rest. Part of a beat gets the longest of an eighth, 16th or 32nd rest
        that starts where it does, in step with the beat, and fits.
    */
    void addRests (std::vector<StaffEvent>& events, int from, int to, int beatsPerMeasure)
    {
        constexpr auto perBeat = Score::slotsPerBeat;

        for (auto slot = from; slot < to;)
        {
            const auto onset = (double) slot / perBeat;
            const auto beat = slot / perBeat;
            const auto beatEmpty = slot % perBeat == 0 && slot + perBeat <= to;

            if (beatEmpty && beat % 2 == 0 && beat + 1 < beatsPerMeasure && slot + 2 * perBeat <= to)
            {
                events.push_back ({ onset, Duration::half, {}, false, false });
                slot += 2 * perBeat;
                continue;
            }

            if (beatEmpty)
            {
                events.push_back ({ onset, Duration::quarter, {}, false, false });
                slot += perBeat;
                continue;
            }

            auto restLength = perBeat / 2;

            while (restLength > 1 && (slot % restLength != 0 || slot + restLength > to))
                restLength /= 2;

            events.push_back ({ onset, getDurationForBeats ((double) restLength / perBeat), {}, false, false });
            slot += restLength;
        }
    }

    /** How many notes a beat has room for: as many as the shortest note or rest needs. */
    void setNotesPerBeat (StaffContent& content)
    {
        auto shortest = 1.0;

        for (const auto& event : content.events)
            shortest = juce::jmin (shortest, getBeats (event.duration));

        content.notesPerBeat = juce::jlimit (1, Score::slotsPerBeat, juce::roundToInt (1.0 / shortest));
    }

    music::Tone shiftedByOctave (music::Tone tone, int octaves)
    {
        tone.pitch.step += 7 * octaves;
        tone.midi += 12 * octaves;
        return tone;
    }

    /** One time round an arpeggio that goes up and back down, or down and back up: through the
        chord's notes, on to the first note's octave, if the chord doesn't reach that far, and back
        through the notes to the second. Played over and over, it fills a measure, e.g. C E G C' G E.
    */
    std::vector<music::Tone> getArpeggioCycle (std::vector<music::Tone> tones, bool upwards)
    {
        std::sort (tones.begin(), tones.end(), [upwards] (const music::Tone& a, const music::Tone& b) { return upwards ? a.midi < b.midi : a.midi > b.midi; });

        if (tones.empty())
            return tones;

        auto cycle = tones;
        const auto turn = shiftedByOctave (tones.front(), upwards ? 1 : -1);
        const auto reachesTurn = upwards ? tones.back().midi >= turn.midi : tones.back().midi <= turn.midi;

        if (! reachesTurn)
            cycle.push_back (turn);

        for (auto i = (int) tones.size() - (reachesTurn ? 2 : 1); i >= 1; --i)
            cycle.push_back (tones[(size_t) i]);

        return cycle;
    }

    StaffContent getChordContent (const Score& score, const music::Chord& chord, const MeasureChord& measureChord)
    {
        StaffContent content;
        content.source = StaffContent::Source::chord;

        const auto beats = score.getBeatsPerMeasure();
        const auto measureLength = beats * Score::slotsPerBeat;
        const auto length = score.getChordNoteLength (measureChord);
        const auto type = measureChord.style.type;
        const auto add = [&] (int slot, std::vector<music::Tone> tones, int slots, bool rolled)
        {
            content.events.push_back ({ (double) slot / Score::slotsPerBeat, getDurationForBeats ((double) slots / Score::slotsPerBeat),
                                        std::move (tones), rolled, false });
        };

        if (! music::isMelodic (type))
        {
            // As long as its note length, from the start of the measure, so it lines up with the
            // first beat of anything on the other staff, and again until the measure's full: the
            // whole measure, unless a shorter length's been chosen.
            auto slot = 0;

            for (; slot + length <= measureLength; slot += length)
                add (slot, chord.tones, length, type == music::ChordType::rolled);

            addRests (content.events, slot, measureLength, beats);
            setNotesPerBeat (content);
            return content;
        }

        // Single notes of the note length, one after another until the measure's full
        const auto room = measureLength / length;
        std::vector<music::Tone> sequence;
        auto holdLast = false;      // whether the last note lasts to the end of the measure

        if (type == music::ChordType::random)
        {
            sequence = getSequence (chord, measureChord);
            sequence.resize ((size_t) juce::jmin ((int) sequence.size(), room));
        }
        else
        {
            // An arpeggio goes up and down, or down and up, again and again. When there's room
            // for it to come round to its first note again, with no more than part of the next
            // time round left, it stops there, and holds that note to the end of the measure.
            const auto cycle = getArpeggioCycle (chord.tones, type == music::ChordType::arpeggioUp);
            const auto period = (int) cycle.size();

            for (int i = 0; i < room && period > 0; ++i)
                sequence.push_back (cycle[(size_t) (i % period)]);

            // It's held only if there's a note as long as what's left of the measure.
            if (period > 1 && room > period)
            {
                const auto notes = (room - 1) / period * period + 1;
                const auto left = measureLength - (notes - 1) * length;

                if (Score::fitNoteLength (left, left) == left)
                {
                    sequence.resize ((size_t) notes);
                    holdLast = true;
                }
            }
        }

        auto slot = 0;

        for (size_t i = 0; i < sequence.size(); ++i)
        {
            const auto last = i + 1 == sequence.size();
            const auto slots = last && holdLast ? measureLength - slot : length;
            add (slot, { sequence[i] }, slots, false);
            slot += slots;
        }

        addRests (content.events, slot, measureLength, beats);
        setNotesPerBeat (content);
        return content;
    }

    StaffContent getNotesContent (const Score& score, int part, Staff staff, int measure, const std::array<int, 7>& keyAlterations)
    {
        // Notes start on 32nd notes: on a beat, or a number of 32nds through one.
        const auto beats = score.getBeatsPerMeasure();
        const auto slots = beats * Score::slotsPerBeat;
        const auto beatOf = [] (int slot) { return (double) slot / Score::slotsPerBeat; };
        const auto isEmpty = [&] (int slot) { return score.getNotes (part, staff, measure, beatOf (slot)).empty(); };
        StaffContent content;

        bool allEmpty = true;

        for (int slot = 0; slot < slots; ++slot)
            allEmpty = allEmpty && isEmpty (slot);

        // An empty measure gets a whole rest in any time signature.
        if (allEmpty)
        {
            content.events.push_back ({ 0.0, Duration::whole, {}, false, true });
            return content;
        }

        for (int slot = 0; slot < slots;)
        {
            const auto onset = beatOf (slot);

            if (! isEmpty (slot))
            {
                // As long as they were added, or as long as fits in what's left of the measure. Any
                // notes they cover, which only a damaged file could have, aren't shown.
                const auto added = (int) std::lround (score.getNoteLength (part, staff, measure, onset) * Score::slotsPerBeat);
                const auto length = Score::fitNoteLength (added, slots - slot);
                StaffEvent event { onset, getDurationForBeats (beatOf (length)), {}, false, false };

                for (const auto& pitch : score.getNotes (part, staff, measure, onset))
                {
                    event.tones.push_back (makeTone (pitch, keyAlterations));

                    if (score.isTied (part, staff, measure, onset, pitch))
                        content.tiedNotes.push_back ({ onset, pitch });
                }

                content.events.push_back (event);
                slot += length;
                continue;
            }

            // Rests up to the next notes, or the end of the measure
            auto end = slot + 1;

            while (end < slots && isEmpty (end))
                ++end;

            addRests (content.events, slot, end, beats);
            slot = end;
        }

        // Eighths and shorter notes need room, and are beamed a beat at a time.
        setNotesPerBeat (content);
        return content;
    }
}

Duration getDurationForBeats (double beats)
{
    return beats >= 4.0 ? Duration::whole
         : beats >= 3.0 ? Duration::dottedHalf
         : beats >= 2.0 ? Duration::half
         : beats >= 1.5 ? Duration::dottedQuarter
         : beats >= 1.0 ? Duration::quarter
         : beats >= 0.75 ? Duration::dottedEighth
         : beats >= 0.5 ? Duration::eighth
         : beats >= 0.25 ? Duration::sixteenth
                         : Duration::thirtySecond;
}

double getBeats (Duration duration)
{
    switch (duration)
    {
        case Duration::whole:       return 4.0;
        case Duration::dottedHalf:  return 3.0;
        case Duration::half:        return 2.0;
        case Duration::dottedQuarter: return 1.5;
        case Duration::quarter:     return 1.0;
        case Duration::dottedEighth: return 0.75;
        case Duration::eighth:      return 0.5;
        case Duration::sixteenth:   return 0.25;
        case Duration::thirtySecond: return 0.125;
    }

    return 1.0;
}

bool isDotted (Duration duration)
{
    return duration == Duration::dottedHalf || duration == Duration::dottedQuarter || duration == Duration::dottedEighth;
}

Duration getFullMeasureDuration (int beatsPerMeasure)
{
    return beatsPerMeasure == 4 ? Duration::whole
         : beatsPerMeasure == 3 ? Duration::dottedHalf
                                : Duration::half;
}

MeasureContent getMeasureContent (const Score& score, int part, int measure)
{
    MeasureContent content;
    std::array<bool, 2> filled { false, false };

    if (const auto* measureChord = score.getChord (part, measure))
    {
        if (auto chord = score.getChordNotes (part, measure))
        {
            const auto& style = measureChord->style;
            content.chord = chord;
            content.chordStyle = style;

            content.staves[(size_t) style.staff] = getChordContent (score, *chord, *measureChord);
            filled[(size_t) style.staff] = true;

            const auto otherStaff = music::getOtherStaff (style.staff);

            if (score.chordUsesStaff (part, measure, otherStaff))
            {
                // Held for the whole measure, from the downbeat, like the chord. With all its notes
                // taken out by hand, it's a whole-measure rest, rather than the quarter notes it hides.
                auto& alternate = content.staves[(size_t) otherStaff];
                alternate.source = StaffContent::Source::alternate;

                if (auto tones = score.getAlternateTones (part, measure); ! tones.empty())
                    alternate.events.push_back ({ 0.0, getFullMeasureDuration (score.getBeatsPerMeasure()), std::move (tones),
                                                  score.getAlternateStaff (part) == music::AlternateStaff::rolledChord, false });
                else
                    alternate.events.push_back ({ 0.0, Duration::whole, {}, false, true });
                filled[(size_t) otherStaff] = true;
            }
        }
    }

    const auto keyAlterations = music::getKeyAlterations (score.getKey());

    for (auto staff : { Staff::treble, Staff::bass })
        if (! filled[(size_t) staff])
            content.staves[(size_t) staff] = getNotesContent (score, part, staff, measure, keyAlterations);

    return content;
}
