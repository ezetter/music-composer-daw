#pragma once

#include "Music.h"

#include <juce_events/juce_events.h>

#include <array>
#include <optional>
#include <vector>

using music::Staff;

/** A quarter note on one beat of one staff. */
struct Note
{
    Staff staff;
    int measure;
    int beat;
    music::Pitch pitch;

    bool operator== (const Note&) const = default;
};

/** How a measure's chord is written: which staff it's on, as what type of chord, and what the
    other staff shows.
*/
struct ChordStyle
{
    Staff staff = Staff::treble;
    music::ChordType type = music::ChordType::block;
    music::AlternateStaff alternate = music::AlternateStaff::none;

    bool operator== (const ChordStyle&) const = default;
};

/** A measure's chord, as the chord panel describes it. */
struct MeasureChord
{
    music::ChordSpec spec;

    /** Notes set on the piano, when they're not simply the spec's own voicing. */
    std::optional<std::vector<music::KeyboardNote>> keyboardNotes;

    ChordStyle style;

    /** The order the Random chord type plays the notes in, as MIDI note numbers. */
    std::vector<int> randomOrder;

    /** The alternate staff's notes, when they've been changed by hand from what the chord gives it. */
    std::optional<std::vector<music::KeyboardNote>> alternateNotes;

    bool hasNotes() const { return ! spec.isEmpty() || keyboardNotes.has_value(); }
};

/** A piece of music on a grand staff, in one key and time signature. Listeners hear about
    every change.
*/
class Score : public juce::ChangeBroadcaster
{
public:
    static constexpr int maxBeatsPerMeasure = 4;
    static constexpr int initialMeasures = 4;

    Score();

    //==============================================================================
    /** The key, as an index into music::getMajorKeys(). */
    int getKeyIndex() const noexcept { return keyIndex; }
    music::Spelling getKey() const;

    /** Changes the key. Chords follow their numerals into the new key, but notes keep their pitches. */
    void setKeyIndex (int);

    /** The number of quarter-note beats in a measure: 2, 3 or 4. */
    int getBeatsPerMeasure() const noexcept { return beatsPerMeasure; }

    /** Changes the time signature. Notes on beats that no longer fit are kept, and come back if
        the measures grow again.
    */
    void setBeatsPerMeasure (int);

    /** The tempo, in quarter notes per minute. */
    double getBeatsPerMinute() const noexcept { return beatsPerMinute; }
    void setBeatsPerMinute (double);

    static constexpr double minBeatsPerMinute = 20.0, maxBeatsPerMinute = 300.0;

    /** How long a measure lasts when it plays, at the tempo. */
    double getSecondsPerMeasure() const noexcept { return beatsPerMeasure * 60.0 / beatsPerMinute; }

    //==============================================================================
    int getNumMeasures() const noexcept { return (int) measures.size(); }
    void addMeasure();

    /** Removes the last measure and everything in it, unless it's the only one. */
    void removeLastMeasure();

    //==============================================================================
    /** Adds a quarter note. Returns false if the score already has it, or its staff is taken by
        the measure's chord.
    */
    bool addNote (const Note&);

    /** The quarter notes on one beat of one staff, lowest first. */
    const std::vector<music::Pitch>& getNotes (Staff, int measure, int beat) const;

    /** Whether a beat of a staff has a quarter note on this line or space, whatever its sharp or flat. */
    bool hasNoteAt (Staff, int measure, int beat, int step) const;

    /** Removes the quarter notes on a line or space of one beat. Returns false if there weren't any. */
    bool removeNotesAt (Staff, int measure, int beat, int step);

    //==============================================================================
    /** The measure's chord, or null if it doesn't have one. */
    const MeasureChord* getChord (int measure) const;

    /** Sets or removes a measure's chord. A chord with notes replaces the quarter notes on the
        staves it uses.
    */
    void setChord (int measure, std::optional<MeasureChord>);

    /** The notes and names of a measure's chord in the current key, if it has any notes. */
    std::optional<music::Chord> getChordNotes (int measure) const;

    /** Whether a measure's chord puts notes on a staff. */
    bool chordUsesStaff (int measure, Staff) const;

    /** Adds a note to a measure's chord, or takes it out if it's already there, as setting the
        chord's notes on the piano does. A measure without a chord gets one in the given style.
    */
    void toggleChordNote (int measure, int midiNote, const ChordStyle& styleForNewChord);

    /** Adds a note to the notes a measure's chord puts on its alternate staff, or takes it out if
        it's already there. From then on the alternate staff keeps these notes, until the chord changes.
    */
    void toggleAlternateNote (int measure, const music::KeyboardNote&);

    /** The notes a measure's chord puts on its alternate staff. */
    std::vector<music::Tone> getAlternateTones (int measure) const;

    /** Picks a new order for a measure's chord to play its notes in, if it's a Random chord. */
    void reshuffle (int measure);

    //==============================================================================
    /** Replaces the score with a new, empty one. */
    void clear();

    /** The whole score, as JSON to save in a file. */
    juce::var toJSON() const;

    /** Replaces the score with one saved by toJSON(). Anything missing or out of range falls
        back to its default, so a damaged file loads as much as it can.
    */
    juce::Result loadJSON (const juce::var&);

private:
    struct Measure
    {
        std::array<std::array<std::vector<music::Pitch>, maxBeatsPerMeasure>, 2> notes;
        std::optional<MeasureChord> chord;
    };

    std::optional<music::Chord> getChordNotes (const MeasureChord&) const;
    void tidyChord (Measure&, bool reshuffleRandomOrder = false);

    std::vector<Measure> measures = std::vector<Measure> (initialMeasures);
    int keyIndex = 0;
    int beatsPerMeasure = 4;
    double beatsPerMinute = 120.0;
    juce::Random random;
};
