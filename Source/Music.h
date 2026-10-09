#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <optional>
#include <variant>
#include <vector>

/** The music theory behind the chord panel: keys, spelling, and building and naming chords.
    It follows the Chord Progression Builder, so the same settings give the same chords.

    Letters are numbered from C = 0 to B = 6, and a letter index counts letters up from C0, so
    middle C is 28. An alteration counts semitones: 1 is a sharp and -1 a flat.
*/
namespace music
{
    /** Floored modulo, which is never negative for a positive divisor. */
    int mod (int n, int divisor);

    /** "♯", "♭", "♯♯" and so on, or nothing for a natural. */
    juce::String getAccidentalText (int alter);

    /** The staves of the grand staff, which are also the clefs a chord can be written in. */
    enum class Staff { treble, bass };

    Staff getOtherStaff (Staff);

    /** A note's name without its octave. */
    struct Spelling
    {
        int letter = 0;
        int alter = 0;

        int getPitchClass() const;
        juce::String getName() const;

        bool operator== (const Spelling&) const = default;
    };

    /** A written note: its letter index and alteration. */
    struct Pitch
    {
        int step = 0;
        int alter = 0;

        int getLetter() const;
        int getOctave() const;
        int getMidiNoteNumber() const;
        Spelling getSpelling() const;

        /** The name with its octave, e.g. "A♭4". */
        juce::String getName() const;

        bool operator== (const Pitch&) const = default;
    };

    //==============================================================================
    /** The 15 major keys, named by their tonics, from C through C♯ to C♭ as the key menu lists them. */
    const std::array<Spelling, 15>& getMajorKeys();

    juce::String getKeyName (Spelling tonic);

    /** The minor key with the same key signature as a major key: its tonic a minor third below. */
    Spelling getRelativeMinor (Spelling majorTonic);

    /** A key: its tonic, and whether it's major or minor. A minor key's scale is the natural
        minor, with the notes of its relative major, so its key signature too.
    */
    struct Key
    {
        Spelling tonic;
        bool minor = false;

        Key (Spelling keyTonic, bool isMinor = false) : tonic (keyTonic), minor (isMinor) {}

        /** The major key with the same key signature: itself, or a minor key's relative major. */
        Spelling getSignatureTonic() const;

        bool operator== (const Key&) const = default;
    };

    /** The key's scale, from its tonic: major, or natural minor. A chord's degree counts from it. */
    std::array<Spelling, 7> getScale (Key);

    /** The key signature's alteration for each letter. */
    std::array<int, 7> getKeyAlterations (Key);

    /** e.g. "No sharps or flats" or "2 flats: B♭ E♭". */
    juce::String getKeySignatureText (Key);

    /** How a key spells a pitch class: as its own scale note if it has one, else as a natural,
        else as a sharp (or a flat in keys with flats).
    */
    Spelling spellPitchClass (Key, int pitchClass);

    /** A MIDI note written as the key spells its pitch class, in the octave it sounds in. */
    Pitch spellMidiNote (Key, int midiNote);

    //==============================================================================
    enum class AddedNote { none, dominant7th, major7th, minor7th, dominant9th, major9th, minor9th };

    struct AddedNoteInfo
    {
        const char* name;        // e.g. "Dominant 7th"
        int seventhSemitones;
        bool hasNinth;
        const char* symbol;      // chord symbol suffix, e.g. "maj7"
        const char* numeral;     // numeral suffix, e.g. "M7"
    };

    const AddedNoteInfo& getInfo (AddedNote);

    /** The added notes that suit a major or a minor chord. */
    std::vector<AddedNote> getAddedNoteChoices (bool minor);

    /** Everything the chord panel sets. */
    struct ChordSpec
    {
        int degree = -1;              // the scale step, from 0 for I to 6 for VII; -1 for no chord
        bool flat = false;            // lowers the root a half step
        bool sharp = false;           // raises it a half step instead, e.g. for a minor key's leading-tone chord, ♯vii°
        bool minor = false;
        bool altered = false;         // augmented on a major chord, diminished on a minor one
        AddedNote addedNote = AddedNote::none;
        int inversion = 0;            // 0 for root position; 3 only with an added note
        int octave = 0;               // -1, 0 or 1

        bool isEmpty() const noexcept { return degree < 0; }

        bool operator== (const ChordSpec&) const = default;
    };

    /** "Root position", "1st inversion" and so on. */
    juce::String getPositionName (int inversion);

    /** A note of a chord. */
    struct Tone
    {
        Pitch pitch;
        int midi = 0;
        bool inKey = true;

        juce::String getName() const;
        juce::String getNameWithOctave() const;
    };

    /** A chord's notes, and the names it's shown with. */
    struct Chord
    {
        juce::String numeral;         // e.g. "♭VII" or "vi"; empty when the notes don't make a chord the panel can describe
        juce::String sign;            // "+", "°" or empty
        juce::String seventhMark;     // "M" on a major 7th or 9th
        juce::StringArray figures;    // figured-bass marks after the numeral, e.g. "6" and "5"
        juce::String label;           // e.g. "V7", "IM7" or "vii°"
        Spelling root;
        std::optional<int> inversion; // empty when the notes don't make a chord the panel can describe
        std::vector<Tone> tones;      // low to high
        juce::String symbol;          // the chord symbol, e.g. "G7/B"
        juce::String name;            // e.g. "G dominant 7th"
        bool fromKeyboard = false;
    };

    /** The chord a spec describes, which mustn't be empty. Its bass note falls in C4-B4 on the
        treble staff and E2-D3 on the bass staff, before the spec's octave moves it.
    */
    Chord createChord (Key, const ChordSpec&, Staff = Staff::treble);

    //==============================================================================
    /** A note set on the piano, with how it's spelled. */
    struct KeyboardNote
    {
        int midi = 0;
        Spelling spelling;

        bool operator== (const KeyboardNote&) const = default;
    };

    /** The tones of notes set by hand, low to high. */
    std::vector<Tone> createTones (Key, const std::vector<KeyboardNote>&);

    /** A chord made of notes set on the piano. If the spec isn't empty, the chord keeps its names. */
    Chord createChord (Key, const std::vector<KeyboardNote>&, const ChordSpec&, Staff);

    /** How the panel describes notes set on the piano. */
    struct NotesDescription
    {
        ChordSpec spec;               // empty if the panel can't describe the notes

        /** The notes, spelled to suit the chord they make; or nothing if they're exactly the
            voicing the spec produces, so the spec alone can stand for them.
        */
        std::optional<std::vector<KeyboardNote>> keyboardNotes;
    };

    NotesDescription describeNotes (Key, Staff, std::vector<int> midiNotes);

    //==============================================================================
    /** What the other staff of the grand staff shows under or over a chord. */
    enum class AlternateStaff { none, root, octave, blockChord, rolledChord };

    std::vector<Tone> getAlternateTones (Key, const Chord&, Staff chordStaff, AlternateStaff);

    /** How a chord is written and played. */
    enum class ChordType { block, arpeggioUp, arpeggioDown, random, rolled };

    /** Whether the chord type writes the chord as single notes. */
    bool isMelodic (ChordType);

    /** How many arpeggio notes go in a beat: quarter notes if they all fit in the measure, else
        eighths, else sixteenths.
    */
    int getArpeggioNotesPerBeat (int numNotes, int beatsPerMeasure);

    /** A random order for a chord's notes that fills the slots: every note at least once, repeats
        shared out evenly, and no note twice in a row.
    */
    std::vector<int> createRandomOrder (const std::vector<int>& midiNotes, int numSlots, juce::Random&);

    //==============================================================================
    /** How loudly to play, softest first. */
    enum class Dynamic { ppp, pp, p, mp, mf, f, ff, fff };

    constexpr std::array<Dynamic, 8> allDynamics { Dynamic::ppp, Dynamic::pp, Dynamic::p, Dynamic::mp,
                                                   Dynamic::mf, Dynamic::f, Dynamic::ff, Dynamic::fff };

    /** How it's written: "ppp", "mf" and so on. */
    juce::String getDynamicMark (Dynamic);

    /** What it stands for: "pianississimo", "mezzo forte" and so on. */
    juce::String getDynamicName (Dynamic);

    /** The dynamic written this way, if any. */
    std::optional<Dynamic> findDynamic (const juce::String& mark);

    /** The MIDI velocity notes play at, spread evenly from 20 for ppp to 127 for fff. */
    int getDynamicVelocity (Dynamic);

    constexpr int softestVelocity = 20, loudestVelocity = 127;

    /** The velocity notes play at before any dynamic's marked, as every note did before there were dynamics. */
    constexpr int unmarkedVelocity = 102;

    /** A crescendo or decrescendo, written as a hairpin: a wedge opening or closing over the notes it spans. */
    enum class Hairpin { crescendo, decrescendo };

    /** How it's called: "Crescendo" or "Decrescendo". */
    juce::String getHairpinName (Hairpin);

    /** How far a hairpin moves the velocity each beat: a crescendo this fraction of the way from
        where it is to the loudest, a decrescendo this fraction of the way to the softest. So a
        crescendo from pp grows faster than one from ff.
    */
    constexpr double hairpinChangePerBeat = 0.1;

    /** The velocity after a hairpin has gone on for some beats from a starting velocity. */
    double applyHairpin (Hairpin, double startVelocity, double beats);

    /** The sustain pedal, whose marks say when it goes down and comes up. */
    enum class Pedal { sustain };

    /** A repeat sign: a start repeat, at the start of a measure, or an end repeat, at its end,
        which sends the music back to the last start repeat the first time it's reached; or first
        and second endings, the first played the first time through and the second after it.
    */
    enum class Repeat { start, end, endings };

    /** Something marked on the score to say how it's played: a dynamic, a hairpin, the pedal, or
        a repeat sign.
    */
    using Marking = std::variant<Dynamic, Hairpin, Pedal, Repeat>;
}
