#pragma once

#include "Score.h"

enum class Duration { whole, dottedHalf, half, quarter, eighth, sixteenth, thirtySecond, dottedQuarter, dottedEighth };

/** Whether a note or rest is written with a dot after it, making it half as long again. */
bool isDotted (Duration);

/** A note, chord or rest on one staff. */
struct StaffEvent
{
    double onset = 0.0;                 // in beats from the start of the measure
    Duration duration = Duration::quarter;
    std::vector<music::Tone> tones;     // empty for a rest
    bool rolled = false;
    bool centred = false;               // written in the middle of the measure rather than at its onset

    bool isRest() const noexcept { return tones.empty(); }
};

/** What one staff has in one measure. */
struct StaffContent
{
    /** Quarter notes, the measure's chord, or the notes its chord puts on the alternate staff. */
    enum class Source { notes, chord, alternate };

    Source source = Source::notes;
    std::vector<StaffEvent> events;     // in time order
    int notesPerBeat = 1;               // 2 for eighths, 4 for 16ths and 8 for 32nds

    /** The notes tied to the next note of the same pitch, by when they start, in beats. */
    std::vector<std::pair<double, music::Pitch>> tiedNotes;
};

/** Everything written in a measure, worked out from the score. */
struct MeasureContent
{
    std::array<StaffContent, 2> staves;
    std::optional<music::Chord> chord;
    ChordStyle chordStyle;
};

/** What one part has in one measure. */
MeasureContent getMeasureContent (const Score&, int part, int measure);

/** A chord written as one note filling the measure: a whole note in 4/4, a dotted half in 3/4
    and a half in 2/4.
*/
Duration getFullMeasureDuration (int beatsPerMeasure);

/** The longest note that lasts no longer than this many beats: a 32nd, 16th, eighth, dotted
    eighth, quarter, dotted quarter, half, dotted half or whole note.
*/
Duration getDurationForBeats (double beats);

/** How many quarter-note beats a note or rest lasts. */
double getBeats (Duration);
