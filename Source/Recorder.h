#pragma once

#include "Score.h"

#include <optional>
#include <vector>

/** Writes the keys played while recording into the score, each as a note as long as the key was
    held, on the treble staff from middle C up and the bass staff below it, in the part it played.

    Keys are given where the score's got to as it's played, its repeats written out, and their
    notes go in the measures as written: a key played the second time through a repeat goes in
    the repeated measure, and one held as the music goes back for a repeat ends there.

    Notes start and end on the nearest 16th note. A note within a measure is written as the length
    nearest to how long it was held; one held over a barline is tied across it. A note still held
    when another starts on its staff ends there, as a staff has one line of notes at a time, and
    notes starting together make a chord.
*/
class Recorder
{
public:
    explicit Recorder (Score&);

    /** A key going down, at a point in the score, in beats from its start, playing a part. */
    void keyDown (int midiNote, double beat, int partId);

    /** A key coming up, which writes its note. A beat before the key went down, as when a loop's
        gone back to the start while it was held, ends the note at the end of the score.
    */
    void keyUp (int midiNote, double beat);

    /** Writes the notes of the keys still down, ending them at a point, or at the end of the score. */
    void finish (std::optional<double> beat);

    /** Forgets the keys still down, without writing them. */
    void clear() noexcept { held.clear(); }

    /** What notes start and end on, in 32nds: a 16th note. */
    static constexpr int grid = 2;

private:
    struct HeldKey
    {
        int midiNote;
        int partId;
        Staff staff;
        double beat;                    // where it went down, from the start of the score
        int start;                      // where its note starts, in 32nds from the start of the score
        std::optional<int> cut;         // where another note on its staff starts, if one has since
    };

    void write (const HeldKey&, double endBeat);

    /** How long the score is, as it's played, its repeats and all, in 32nds. */
    int getScoreSlots() const;

    Score& score;
    std::vector<HeldKey> held;
};
