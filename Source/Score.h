#pragma once

#include <juce_events/juce_events.h>

#include <array>
#include <vector>

/** The two staves of the grand staff. */
enum class Staff { treble, bass };

/** A quarter note on one beat of one staff.

    Pitches are diatonic steps counted up from C0, so middle C (C4) is 28. There are no
    accidentals yet, so every pitch is a natural.
*/
struct Note
{
    Staff staff;
    int measure;
    int beat;
    int pitch;

    bool operator== (const Note&) const = default;
};

/** Music in 4/4 time for a grand staff. Listeners hear about every change. */
class Score : public juce::ChangeBroadcaster
{
public:
    static constexpr int beatsPerMeasure = 4;
    static constexpr int initialMeasures = 4;

    int getNumMeasures() const noexcept { return (int) measures.size(); }

    void addMeasure();

    /** Removes the last measure and its notes, unless it's the only one. */
    void removeLastMeasure();

    /** Adds a note, or returns false if the score already has it. */
    bool addNote (const Note&);

    /** The pitches of the notes on one beat of one staff, lowest first. */
    const std::vector<int>& getChord (Staff, int measure, int beat) const;

private:
    using Chord = std::vector<int>;
    using Measure = std::array<std::array<Chord, beatsPerMeasure>, 2>;

    std::vector<Measure> measures = std::vector<Measure> (initialMeasures);
};
