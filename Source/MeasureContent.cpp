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

    StaffContent getChordContent (const music::Chord& chord, const MeasureChord& measureChord, int beatsPerMeasure)
    {
        StaffContent content;
        content.source = StaffContent::Source::chord;

        if (! music::isMelodic (measureChord.style.type))
        {
            // Held for the whole measure, written at its start so it lines up with the first
            // beat of anything on the other staff.
            content.events.push_back ({ 0.0, getFullMeasureDuration (beatsPerMeasure), chord.tones,
                                        measureChord.style.type == music::ChordType::rolled, false });
            return content;
        }

        // Single notes, padded with rests of the same value to fill the measure.
        content.notesPerBeat = music::getArpeggioNotesPerBeat ((int) chord.tones.size(), beatsPerMeasure);
        const auto duration = content.notesPerBeat == 1 ? Duration::quarter
                            : content.notesPerBeat == 2 ? Duration::eighth
                                                        : Duration::sixteenth;
        const auto sequence = getSequence (chord, measureChord);
        const auto numSlots = juce::jmax ((int) sequence.size(), beatsPerMeasure * content.notesPerBeat);

        for (int slot = 0; slot < numSlots; ++slot)
        {
            StaffEvent event { (double) slot / content.notesPerBeat, duration, {}, false, false };

            if (slot < (int) sequence.size())
                event.tones.push_back (sequence[(size_t) slot]);

            content.events.push_back (event);
        }

        return content;
    }

    StaffContent getNotesContent (const Score& score, int part, Staff staff, int measure, const std::array<int, 7>& keyAlterations)
    {
        const auto beats = score.getBeatsPerMeasure();
        StaffContent content;

        const auto isEmpty = [&] (int beat) { return score.getNotes (part, staff, measure, beat).empty(); };

        bool allEmpty = true;

        for (int beat = 0; beat < beats; ++beat)
            allEmpty = allEmpty && isEmpty (beat);

        // An empty measure gets a whole rest in any time signature.
        if (allEmpty)
        {
            content.events.push_back ({ 0.0, Duration::whole, {}, false, true });
            return content;
        }

        for (int beat = 0; beat < beats; ++beat)
        {
            if (! isEmpty (beat))
            {
                StaffEvent event { (double) beat, Duration::quarter, {}, false, false };

                for (const auto& pitch : score.getNotes (part, staff, measure, beat))
                    event.tones.push_back (makeTone (pitch, keyAlterations));

                content.events.push_back (event);
                continue;
            }

            // Two empty beats that make up the first half of 4/4 or 3/4, or the second half of
            // 4/4, share a half rest.
            if (beat % 2 == 0 && beat + 1 < beats && isEmpty (beat + 1))
            {
                content.events.push_back ({ (double) beat, Duration::half, {}, false, false });
                ++beat;
                continue;
            }

            content.events.push_back ({ (double) beat, Duration::quarter, {}, false, false });
        }

        return content;
    }
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

            content.staves[(size_t) style.staff] = getChordContent (*chord, *measureChord, score.getBeatsPerMeasure());
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
