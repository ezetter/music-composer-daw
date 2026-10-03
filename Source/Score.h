#pragma once

#include "Music.h"

#include <juce_events/juce_events.h>

#include <array>
#include <cmath>
#include <optional>
#include <vector>

using music::Staff;

/** A note on one staff, in one of the score's parts, starting on a beat or on a 32nd note between beats. */
struct Note
{
    Staff staff;
    int measure;
    double beat;        // from the start of the measure, in steps of a 32nd note: 0, 0.125, 0.25...
    music::Pitch pitch;
    int part = 0;
    double length = 1;  // in beats: 0.125 for a 32nd note, 0.25 for a 16th, 0.5 for an eighth, 0.75 for a dotted eighth, 1 for a quarter,
                        // 1.5 for a dotted quarter, 2 for a half, 3 for a dotted half, 4 for a whole

    bool operator== (const Note& other) const
    {
        // Beats and lengths are whole numbers of 32nd notes, so they're compared exactly.
        return staff == other.staff && measure == other.measure && juce::exactlyEqual (beat, other.beat)
            && pitch == other.pitch && part == other.part && juce::exactlyEqual (length, other.length);
    }
};

/** A crescendo or decrescendo in one of a score's parts, starting on a beat or halfway through one. */
struct HairpinMark
{
    int measure;
    double beat;            // from the start of the measure, in steps of a 32nd note
    music::Hairpin type;
    double length;          // in beats, a whole number of 32nd notes, which can run on into later measures

    bool operator== (const HairpinMark& other) const
    {
        return measure == other.measure && juce::exactlyEqual (beat, other.beat) && type == other.type
            && juce::exactlyEqual (length, other.length);
    }
};

/** How a measure's chord is written: which staff it's on, as what type of chord, and in notes
    how long. What the other staff shows is up to the score, for every chord alike.
*/
struct ChordStyle
{
    Staff staff = Staff::treble;
    music::ChordType type = music::ChordType::block;

    /** How long the chord's notes are written, in 32nds, as chosen in the chord window. Without
        one chosen, they're as long as the chord type makes them: see Score::getChordNoteLength().
    */
    std::optional<int> noteLength;

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

/** A piece of music in any number of parts, two to start with, each on its own grand staff and
    played on its own instrument. The parts share the key, time signature, tempo and measures, but
    each has its own notes, chords, dynamics and alternate staff. Listeners hear about every change.

    Parts are numbered from 0, from the top, and the numbers move up when one's taken out. Each
    also has an id, which stays the same as long as it's in the score, for keeping track of it.
*/
class Score : public juce::ChangeBroadcaster
{
public:
    static constexpr int maxBeatsPerMeasure = 4;

    /** Notes start on 32nd notes: on a beat, or a number of 32nds through one. */
    static constexpr int slotsPerBeat = 8;
    static constexpr int maxSlotsPerMeasure = maxBeatsPerMeasure * slotsPerBeat;
    static constexpr int initialMeasures = 4;
    static constexpr int initialParts = 2;

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
    /** How many parts there are: always at least one. */
    int getNumParts() const noexcept { return (int) parts.size(); }

    /** Adds an empty part after the last, with as many measures as the others. Returns its number. */
    int addPart();

    /** Takes a part out, with everything in it, unless it's the only one. The parts after it move up. */
    void removePart (int part);

    /** A part's id, which stays the same while it's in the score, wherever it moves to. */
    int getPartId (int part) const noexcept { return parts[(size_t) part].id; }

    /** The number of the part with an id, or -1 if it's not in the score any more. */
    int findPart (int id) const noexcept;

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
    /** Adds a note, as long as it asks, or as long as fits in what's left of the measure: the
        longest of a whole, dotted half, half, dotted quarter, quarter, dotted eighth, eighth, 16th
        or 32nd note that does. It takes the place
        of any notes it covers, and cuts short a longer note it starts during. The notes starting
        together all have the same length, so they take the new one's. Returns false if the score
        already has it, or its staff is taken by the measure's chord.

        Beats are counted from 0, and notes start on them or on a 32nd note between them.
    */
    bool addNote (const Note&);

    /** The notes starting at one point of one staff, lowest first. */
    const std::vector<music::Pitch>& getNotes (int part, Staff, int measure, double beat) const;

    /** How many beats the notes starting at a point last, as they were added. A shorter time
        signature can leave less room than this, and they're shortened to fit when they're shown.
    */
    double getNoteLength (int part, Staff, int measure, double beat) const;

    static constexpr double maxNoteLength = 4.0;

    /** The longest note that's no longer than asked, and fits in the room there is, both in
        32nd notes: a whole, dotted half, half, dotted quarter, quarter, dotted eighth, eighth, 16th
        or 32nd note.
    */
    static int fitNoteLength (int slots, int room);

    //==============================================================================
    /** Where the notes starting at a point end: the measure and beat the next notes would start
        on, or nothing if that's past the end of the score.
    */
    std::optional<std::pair<int, double>> getFollowingBeat (int part, Staff, int measure, double beat) const;

    /** Whether a note is tied to the note of the same pitch that starts just as it ends, which
        it's held on through. A tie can cross a barline.
    */
    bool isTied (int part, Staff, int measure, double beat, music::Pitch) const;

    /** Ties two notes of the same pitch together, either way round, or unties them if they're
        tied. They have to follow one another: the second starting just as the first ends.
        Returns false, changing nothing, if they can't be tied.
    */
    bool toggleTie (int part, Staff, int measure, double beat, int otherMeasure, double otherBeat, music::Pitch);

    /** Whether a staff has a note starting at a point on this line or space, whatever its sharp or flat. */
    bool hasNoteAt (int part, Staff, int measure, double beat, int step) const;

    /** Removes the notes starting at a point on a line or space. Returns false if there weren't any. */
    bool removeNotesAt (int part, Staff, int measure, double beat, int step);

    //==============================================================================
    /** The dynamic marked at a point of a part, if there's one there. Each part has its own
        dynamics, written between its staves, on a beat or halfway through one.
    */
    std::optional<music::Dynamic> getDynamic (int part, int measure, double beat) const;

    /** Marks a dynamic at a point of a part, replacing any already there, or takes it out. */
    void setDynamic (int part, int measure, double beat, std::optional<music::Dynamic>);

    /** The dynamic a part plays at, at a point: the last one marked there or before it, or none
        if there isn't one. Dynamics on beats a shorter time signature leaves out don't count.
    */
    std::optional<music::Dynamic> getDynamicInForce (int part, int measure, double beat) const;

    /** A part's crescendos and decrescendos, in order, as long as the score has room for: one
        running on past the end stops there. Ones starting on beats a shorter time signature
        leaves out aren't included, but are kept.
    */
    std::vector<HairpinMark> getHairpins (int part) const;

    /** Marks a crescendo or decrescendo, at least an eighth note long and no longer than the
        score has room for. It takes the place of the part's other hairpins it overlaps, including
        one starting at the same point.
    */
    void setHairpin (int part, const HairpinMark&);

    /** Takes out the hairpin starting at a point, if there is one. */
    void removeHairpin (int part, int measure, double beat);

    /** How hard a part's notes are played at a point, as MIDI velocity. A dynamic sets it, until
        the next one, and a hairpin moves it each beat it lasts, from wherever it was towards
        the loudest or softest; after the hairpin it stays where the hairpin left it. Before any
        dynamic it's music::unmarkedVelocity.
    */
    int getVelocity (int part, int measure, double beat) const;

    //==============================================================================
    /** The measure's chord, or null if it doesn't have one. */
    const MeasureChord* getChord (int part, int measure) const;

    /** Sets or removes a measure's chord. A chord with notes replaces the quarter notes on its
        staff, and hides the ones on the alternate staff, if there is one.
    */
    void setChord (int part, int measure, std::optional<MeasureChord>);

    /** Takes out a measure's chord, and puts these notes on its staves instead of anything they
        had, e.g. to write the chord's notes out one by one, so they can be changed on their own.
    */
    void replaceChordWithNotes (int part, int measure, const std::vector<Note>&);

    /** The notes and names of a measure's chord in the current key, if it has any notes. */
    std::optional<music::Chord> getChordNotes (int part, int measure) const;

    /** How long a chord's notes are written, in 32nds, in this time signature: the length chosen
        for it, if it fits in a measure, or the longest that does. Without one chosen, a block or
        rolled chord lasts the measure, and the others' notes are quarters, or eighths or 16ths if
        there are too many notes for quarters to fit.
    */
    int getChordNoteLength (const MeasureChord&) const;

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

    /** Whether a part has nothing in it at all: no notes, chords, dynamics or hairpins. */
    bool isPartEmpty (int part) const;

    /** Whether a part has any chords with notes. */
    bool hasChords (int part) const;

    /** Gives one part the other's chord progression: measure by measure, the same chords, or
        none, and no quarter notes of its own. Its alternate staff setting and its dynamics stay
        as they are; notes set by hand on the alternate staff come too if both parts' alternate
        staffs are the same.
    */
    void copyChords (int fromPart, int toPart);

    //==============================================================================
    /** Replaces the score with a new, empty one, with two empty parts. They keep the ids the
        first two parts had, as being the same parts, emptied.
    */
    void clear();

    /** The whole score, as JSON to save in a file. With the parts' ids, it's the score exactly
        as it is, e.g. for undoing back to it; a file doesn't need them.
    */
    juce::var toJSON (bool withPartIds = false) const;

    /** Replaces the score with one saved by toJSON(). Anything missing or out of range falls
        back to its default, so a damaged file loads as much as it can.

        Parts saved with their ids get them back. Ones without, as in a file, take the id of the
        part that was at their place, as being the same parts with new music, or new ones.
    */
    juce::Result loadJSON (const juce::var&);

private:
    /** A hairpin, where it starts: which it is and how many 32nd notes it lasts. */
    struct HairpinStart
    {
        music::Hairpin type;
        int length;

        bool operator== (const HairpinStart&) const = default;
    };

    struct Measure
    {
        Measure()
        {
            for (auto& staffLengths : lengths)
                staffLengths.fill (slotsPerBeat);
        }

        // By 32nd note: the notes starting there, how many 32nds they last, and which are tied to the next
        std::array<std::array<std::vector<music::Pitch>, maxSlotsPerMeasure>, 2> notes;
        std::array<std::array<int, maxSlotsPerMeasure>, 2> lengths;
        std::array<std::array<std::vector<music::Pitch>, maxSlotsPerMeasure>, 2> ties;
        std::array<std::optional<music::Dynamic>, maxSlotsPerMeasure> dynamics;      // by 32nd note, for both staves
        std::array<std::optional<HairpinStart>, maxSlotsPerMeasure> hairpins;      // starting at each 32nd
        std::optional<MeasureChord> chord;

        /** Takes out all of a staff's notes. */
        void clearNotes (Staff staff)
        {
            for (auto& slot : notes[(size_t) staff])
                slot.clear();

            for (auto& slot : ties[(size_t) staff])
                slot.clear();

            lengths[(size_t) staff].fill (slotsPerBeat);
        }
    };

    struct Part
    {
        int id = 0;
        std::vector<Measure> measures = std::vector<Measure> (initialMeasures);
        music::AlternateStaff alternateStaff = music::AlternateStaff::none;
    };

    Measure& getMeasure (int part, int measure);
    const Measure& getMeasure (int part, int measure) const;
    void tidyChord (Measure&, bool reshuffleRandomOrder = false);

    /** Lets go of ties whose notes aren't there any more, or don't follow one another now. */
    void pruneTies();

    int getSlotsPerMeasure() const noexcept { return beatsPerMeasure * slotsPerBeat; }
    static int toSlot (double beat) { return (int) std::lround (beat * slotsPerBeat); }
    static double toBeats (int slots) { return (double) slots / slotsPerBeat; }
    std::optional<std::pair<int, int>> getFollowingSlot (int part, Staff, int measure, int slot) const;

    std::vector<Part> parts;
    int nextPartId = 1;
    int keyIndex = 0;
    int beatsPerMeasure = 4;
    double beatsPerMinute = 120.0;
    juce::Random random;
};
