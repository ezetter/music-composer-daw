#pragma once

#include "Music.h"

#include <juce_events/juce_events.h>

#include <array>
#include <optional>
#include <vector>

using music::Staff;

/** A note starting on one beat of one staff, in one of the score's parts. */
struct Note
{
    Staff staff;
    int measure;
    int beat;
    music::Pitch pitch;
    int part = 0;
    int length = 1;     // in beats: 1 for a quarter note, 2 for a half note, 4 for a whole note

    bool operator== (const Note&) const = default;
};

/** How a measure's chord is written: which staff it's on, and as what type of chord. What the
    other staff shows is up to the score, for every chord alike.
*/
struct ChordStyle
{
    Staff staff = Staff::treble;
    music::ChordType type = music::ChordType::block;

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

    bool operator== (const MeasureChord&) const = default;
};

/** A piece of music in two parts, each on its own grand staff and played on its own
    instrument. The parts share the key, time signature, tempo and measures, but each has its
    own notes, chords and alternate staff. Listeners hear about every change.
*/
class Score : public juce::ChangeBroadcaster
{
public:
    static constexpr int maxBeatsPerMeasure = 4;
    static constexpr int initialMeasures = 4;
    static constexpr int numParts = 2;

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

    /** What the staff a chord isn't on shows, for every chord in a part. */
    music::AlternateStaff getAlternateStaff (int part) const noexcept { return parts[(size_t) part].alternateStaff; }

    /** Changes what the alternate staff shows, for every chord in a part. Notes set by hand on
        the alternate staff go back to what their chords give it. Quarter notes on the alternate
        staff in a chord's measure are hidden rather than lost, and come back with None.
    */
    void setAlternateStaff (int part, music::AlternateStaff);

    /** How long a measure lasts when it plays, at the tempo. */
    double getSecondsPerMeasure() const noexcept { return beatsPerMeasure * 60.0 / beatsPerMinute; }

    //==============================================================================
    /** The number of measures, which every part has. */
    int getNumMeasures() const noexcept { return (int) parts[0].measures.size(); }
    void addMeasure();

    /** Removes the last measure and everything in it, in every part, unless it's the only one. */
    void removeLastMeasure();

    /** Repeats all the measures after the last one, in every part, with everything in them:
        4 measures become 8, the second 4 a copy of the first, to be edited on their own.
    */
    void cloneMeasures();

    //==============================================================================
    /** Adds a note, as long as it asks, or as fits in what's left of the measure. It takes the
        place of any notes on the beats it covers, and cuts short a longer note it starts during.
        The notes starting on the same beat all have the same length, so they take the new one's.
        Returns false if the score already has it, or its staff is taken by the measure's chord.
    */
    bool addNote (const Note&);

    /** The notes starting on one beat of one staff, lowest first. */
    const std::vector<music::Pitch>& getNotes (int part, Staff, int measure, int beat) const;

    /** How many beats the notes starting on a beat last, as they were added. A shorter time
        signature can leave less room than this, and they're shortened to fit when they're shown.
    */
    int getNoteLength (int part, Staff, int measure, int beat) const;

    static constexpr int maxNoteLength = 4;

    //==============================================================================
    /** Where the notes starting on a beat end: the measure and beat the next notes would start
        on, or nothing if that's past the end of the score.
    */
    std::optional<std::pair<int, int>> getFollowingBeat (int part, Staff, int measure, int beat) const;

    /** Whether a note is tied to the note of the same pitch that starts just as it ends, which
        it's held on through. A tie can cross a barline.
    */
    bool isTied (int part, Staff, int measure, int beat, music::Pitch) const;

    /** Ties two notes of the same pitch together, either way round, or unties them if they're
        tied. They have to follow one another: the second starting just as the first ends.
        Returns false, changing nothing, if they can't be tied.
    */
    bool toggleTie (int part, Staff, int measure, int beat, int otherMeasure, int otherBeat, music::Pitch);

    /** Whether a beat of a staff has a quarter note on this line or space, whatever its sharp or flat. */
    bool hasNoteAt (int part, Staff, int measure, int beat, int step) const;

    /** Removes the quarter notes on a line or space of one beat. Returns false if there weren't any. */
    bool removeNotesAt (int part, Staff, int measure, int beat, int step);

    //==============================================================================
    /** The measure's chord, or null if it doesn't have one. */
    const MeasureChord* getChord (int part, int measure) const;

    /** Sets or removes a measure's chord. A chord with notes replaces the quarter notes on its
        staff, and hides the ones on the alternate staff, if there is one.
    */
    void setChord (int part, int measure, std::optional<MeasureChord>);

    /** The notes and names of a measure's chord in the current key, if it has any notes. */
    std::optional<music::Chord> getChordNotes (int part, int measure) const;

    /** The notes and names a chord would have in this score's key, if it has any notes. */
    std::optional<music::Chord> getChordNotes (const MeasureChord&) const;

    /** Adds a note to a chord, or takes it out, in this score's key, as setting the chord's notes
        on the piano does.
    */
    void toggleChordNote (MeasureChord&, int midiNote) const;

    /** Whether a measure's chord puts notes on a staff, as the chord or the alternate staff. */
    bool chordUsesStaff (int part, int measure, Staff) const;

    /** Adds a note to a measure's chord, or takes it out if it's already there, as setting the
        chord's notes on the piano does. A measure without a chord gets one in the given style.
    */
    void toggleChordNote (int part, int measure, int midiNote, const ChordStyle& styleForNewChord);

    /** Adds a note to the notes a measure's chord puts on its alternate staff, or takes it out if
        it's already there. From then on the alternate staff keeps these notes, until the chord changes.
    */
    void toggleAlternateNote (int part, int measure, const music::KeyboardNote&);

    /** The notes a measure's chord puts on its alternate staff. */
    std::vector<music::Tone> getAlternateTones (int part, int measure) const;

    /** Picks a new order for a measure's chord to play its notes in, if it's a Random chord. */
    void reshuffle (int part, int measure);

    //==============================================================================
    /** Whether a part has any quarter notes, even ones hidden by the time signature or the
        alternate staff, or any chords with notes.
    */
    bool hasNotes (int part) const;

    /** Whether a part has any chords with notes. */
    bool hasChords (int part) const;

    /** Gives one part the other's chord progression: measure by measure, the same chords, or
        none, and no quarter notes of its own. Its alternate staff setting stays as it is; notes
        set by hand on the alternate staff come too if both parts' alternate staffs are the same.
    */
    void copyChords (int fromPart, int toPart);

    //==============================================================================
    /** Replaces the score with a new, empty one, with nothing in either part. */
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
        std::array<std::array<int, maxBeatsPerMeasure>, 2> lengths { { { 1, 1, 1, 1 }, { 1, 1, 1, 1 } } };     // in beats
        std::array<std::array<std::vector<music::Pitch>, maxBeatsPerMeasure>, 2> ties;    // the notes tied to the next
        std::optional<MeasureChord> chord;

        /** Takes out all of a staff's notes. */
        void clearNotes (Staff staff)
        {
            for (auto& beat : notes[(size_t) staff])
                beat.clear();

            for (auto& beat : ties[(size_t) staff])
                beat.clear();

            lengths[(size_t) staff].fill (1);
        }
    };

    struct Part
    {
        std::vector<Measure> measures = std::vector<Measure> (initialMeasures);
        music::AlternateStaff alternateStaff = music::AlternateStaff::none;
    };

    Measure& getMeasure (int part, int measure);
    const Measure& getMeasure (int part, int measure) const;
    void tidyChord (Measure&, bool reshuffleRandomOrder = false);

    /** Lets go of ties whose notes aren't there any more, or don't follow one another now. */
    void pruneTies();

    std::array<Part, numParts> parts;
    int keyIndex = 0;
    int beatsPerMeasure = 4;
    double beatsPerMinute = 120.0;
    juce::Random random;
};
