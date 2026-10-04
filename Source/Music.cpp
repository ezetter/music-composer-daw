#include "Music.h"

#include <algorithm>
#include <cmath>

namespace music
{
namespace
{
    constexpr std::array<int, 7> naturalPitchClasses { 0, 2, 4, 5, 7, 9, 11 };
    constexpr std::array<int, 7> majorScaleSteps { 0, 2, 4, 5, 7, 9, 11 };
    constexpr std::array<int, 7> minorScaleSteps { 0, 2, 3, 5, 7, 8, 10 };      // the natural minor
    constexpr std::array<const char*, 7> letterNames { "C", "D", "E", "F", "G", "A", "B" };
    constexpr std::array<const char*, 7> degreeNumerals { "I", "II", "III", "IV", "V", "VI", "VII" };

    juce::String getSymbol (juce::juce_wchar character)
    {
        return juce::String::charToString (character);
    }

    const juce::juce_wchar sharp = 0x266F, flat = 0x266D, degreeSign = 0x00B0, slashedO = 0x00F8, enDash = 0x2013;

    int floorDivide (int n, int divisor)
    {
        return (n - mod (n, divisor)) / divisor;
    }

    /** Spells a pitch class on a letter, which may be given as a letter index. */
    Spelling spell (int letterIndex, int pitchClass)
    {
        const auto letter = mod (letterIndex, 7);
        auto alter = mod (pitchClass - naturalPitchClasses[(size_t) letter], 12);

        if (alter > 6)
            alter -= 12;

        return { letter, alter };
    }

    // The bottom of each clef's range for chords, and the note on each staff's bottom line, as
    // letter indexes.
    int getLowestChordLetter (Staff staff) { return staff == Staff::treble ? 28 : 16; }
    int getBottomLineLetter (Staff staff)  { return staff == Staff::treble ? 30 : 18; }

    /** A chord member: how many letters and semitones it is above the root. */
    struct Member
    {
        int steps;
        int semitones;
    };

    // Figured-bass marks for each inversion of a triad and of a 7th chord.
    const std::array<juce::StringArray, 3> triadFigures { juce::StringArray {}, juce::StringArray { "6" }, juce::StringArray { "6", "4" } };
    const std::array<juce::StringArray, 4> seventhFigures { juce::StringArray { "7" }, juce::StringArray { "6", "5" },
                                                            juce::StringArray { "4", "3" }, juce::StringArray { "4", "2" } };

    /** The chord shapes that notes set on the piano can be recognised as. */
    struct ChordShape
    {
        const char* name;
        juce::String suffix;
        std::vector<Member> members;
        std::optional<ChordSpec> spec;    // how the panel describes the shape, if it can
    };

    ChordSpec makeSpec (bool minor, bool altered, AddedNote addedNote)
    {
        ChordSpec spec;
        spec.minor = minor;
        spec.altered = altered;
        spec.addedNote = addedNote;
        return spec;
    }

    const std::vector<ChordShape>& getChordShapes()
    {
        static const std::vector<ChordShape> shapes
        {
            { "major",               "",                                   { { 0, 0 }, { 2, 4 }, { 4, 7 } },            makeSpec (false, false, AddedNote::none) },
            { "minor",               "m",                                  { { 0, 0 }, { 2, 3 }, { 4, 7 } },            makeSpec (true,  false, AddedNote::none) },
            { "diminished",          getSymbol (degreeSign),               { { 0, 0 }, { 2, 3 }, { 4, 6 } },            makeSpec (true,  true,  AddedNote::none) },
            { "augmented",           "+",                                  { { 0, 0 }, { 2, 4 }, { 4, 8 } },            makeSpec (false, true,  AddedNote::none) },
            { "dominant 7th",        "7",                                  { { 0, 0 }, { 2, 4 }, { 4, 7 }, { 6, 10 } }, makeSpec (false, false, AddedNote::dominant7th) },
            { "major 7th",           "maj7",                               { { 0, 0 }, { 2, 4 }, { 4, 7 }, { 6, 11 } }, makeSpec (false, false, AddedNote::major7th) },
            { "minor 7th",           "m7",                                 { { 0, 0 }, { 2, 3 }, { 4, 7 }, { 6, 10 } }, makeSpec (true,  false, AddedNote::minor7th) },
            { "half-diminished 7th", getSymbol (slashedO) + "7",           { { 0, 0 }, { 2, 3 }, { 4, 6 }, { 6, 10 } }, std::nullopt },
            { "diminished 7th",      getSymbol (degreeSign) + "7",         { { 0, 0 }, { 2, 3 }, { 4, 6 }, { 6, 9 } },  std::nullopt },
            { "minor-major 7th",     "m(maj7)",                            { { 0, 0 }, { 2, 3 }, { 4, 7 }, { 6, 11 } }, std::nullopt },
            { "dominant 9th",        "9",                                  { { 0, 0 }, { 2, 4 }, { 4, 7 }, { 6, 10 }, { 8, 14 } }, makeSpec (false, false, AddedNote::dominant9th) },
            { "major 9th",           "maj9",                               { { 0, 0 }, { 2, 4 }, { 4, 7 }, { 6, 11 }, { 8, 14 } }, makeSpec (false, false, AddedNote::major9th) },
            { "minor 9th",           "m9",                                 { { 0, 0 }, { 2, 3 }, { 4, 7 }, { 6, 10 }, { 8, 14 } }, makeSpec (true,  false, AddedNote::minor9th) },
            { "suspended 2nd",       "sus2",                               { { 0, 0 }, { 1, 2 }, { 4, 7 } },            std::nullopt },
            { "suspended 4th",       "sus4",                               { { 0, 0 }, { 3, 5 }, { 4, 7 } },            std::nullopt },
        };

        return shapes;
    }

    /** What some notes are, low to high: a single note, an interval (any two notes, or two pitch
        classes), one pitch class in several octaves, a chord shape, or unknown.
    */
    struct Identification
    {
        enum class Kind { note, interval, octaves, chord, unknown };

        Kind kind = Kind::unknown;
        int rootPitchClass = 0;
        const ChordShape* shape = nullptr;
        bool omitsFifth = false;
        int inversion = 0;
    };

    Identification identifyNotes (const std::vector<int>& midiNotes)
    {
        std::vector<int> pitchClasses;

        for (auto midi : midiNotes)
            if (std::find (pitchClasses.begin(), pitchClasses.end(), mod (midi, 12)) == pitchClasses.end())
                pitchClasses.push_back (mod (midi, 12));

        using Kind = Identification::Kind;

        if (midiNotes.size() == 1)     return { Kind::note };
        if (midiNotes.size() == 2)     return { Kind::interval };
        if (pitchClasses.size() == 1)  return { Kind::octaves };
        if (pitchClasses.size() == 2)  return { Kind::interval };

        // Exact shapes first, then 7th chords without their perfect 5th, a common voicing that keeps
        // the chord's name. The bass is tried as the root first, so C-D-G is Csus2 rather than Gsus4.
        for (auto omitFifth : { false, true })
        {
            for (auto rootPitchClass : pitchClasses)
            {
                std::vector<int> intervals;

                for (auto pitchClass : pitchClasses)
                    intervals.push_back (mod (pitchClass - rootPitchClass, 12));

                std::sort (intervals.begin(), intervals.end());

                for (const auto& shape : getChordShapes())
                {
                    if (omitFifth && ! (shape.members.size() >= 4 && shape.members[2].semitones == 7))
                        continue;

                    std::vector<int> shapeIntervals;

                    for (const auto& member : shape.members)
                        if (! omitFifth || member.semitones != 7)
                            shapeIntervals.push_back (member.semitones % 12);

                    std::sort (shapeIntervals.begin(), shapeIntervals.end());

                    if (shapeIntervals != intervals)
                        continue;

                    const auto bass = mod (pitchClasses.front() - rootPitchClass, 12);
                    const auto inversion = std::find_if (shape.members.begin(), shape.members.end(),
                                                         [bass] (const Member& m) { return m.semitones % 12 == bass; });

                    return { Kind::chord, rootPitchClass, &shape, omitFifth, (int) std::distance (shape.members.begin(), inversion) };
                }
            }
        }

        return { Kind::unknown };
    }

    juce::String getOrdinal (int n)
    {
        const auto lastTwo = n % 100;

        if (lastTwo >= 11 && lastTwo <= 13)
            return juce::String (n) + "th";

        const char* const suffixes[] { "th", "st", "nd", "rd" };
        const auto last = n % 10;
        return juce::String (n) + (last >= 0 && last < 4 ? suffixes[last] : "th");
    }

    /** The interval between two tones, e.g. "Major 3rd", "Perfect 5th" or "Minor 10th". */
    juce::String getIntervalName (const Tone& lower, const Tone& upper)
    {
        const auto number = upper.pitch.step - lower.pitch.step + 1;
        const auto simple = mod (number - 1, 7) + 1;
        const auto semitones = upper.midi - lower.midi - 12 * floorDivide (number - 1, 7);
        const auto offset = semitones - std::array<int, 8> { 0, 0, 2, 4, 5, 7, 9, 11 }[(size_t) simple];

        juce::String quality;

        if (simple == 1 || simple == 4 || simple == 5)
            quality = offset == -1 ? "Diminished" : offset == 0 ? "Perfect" : offset == 1 ? "Augmented" : "";
        else
            quality = offset == -2 ? "Diminished" : offset == -1 ? "Minor" : offset == 0 ? "Major" : offset == 1 ? "Augmented" : "";

        if (quality == "Perfect" && simple == 1 && number > 1)
        {
            const auto octaves = (number - 1) / 7;
            const char* const counts[] { "", "Octave", "Two octaves", "Three octaves", "Four octaves", "Five octaves" };
            return octaves < 6 ? juce::String (counts[octaves]) : juce::String (octaves) + " octaves";
        }

        const auto size = number == 1 ? juce::String ("unison") : getOrdinal (number);
        return quality.isNotEmpty() ? quality + " " + size : size.substring (0, 1).toUpperCase() + size.substring (1);
    }

    Tone makeTone (const KeyboardNote& note, const std::array<int, 7>& keyAlterations)
    {
        const auto& spelling = note.spelling;
        const auto octave = floorDivide (note.midi - spelling.alter - naturalPitchClasses[(size_t) spelling.letter], 12) - 1;

        return { { octave * 7 + spelling.letter, spelling.alter }, note.midi,
                 spelling.alter == keyAlterations[(size_t) spelling.letter] };
    }

    std::vector<int> getMidiNotes (const std::vector<Tone>& tones)
    {
        std::vector<int> midiNotes;

        for (const auto& tone : tones)
            midiNotes.push_back (tone.midi);

        return midiNotes;
    }
}

//==============================================================================
int mod (int n, int divisor)
{
    return ((n % divisor) + divisor) % divisor;
}

juce::String getAccidentalText (int alter)
{
    return juce::String::repeatedString (getSymbol (alter > 0 ? sharp : flat), std::abs (alter));
}

Staff getOtherStaff (Staff staff)
{
    return staff == Staff::treble ? Staff::bass : Staff::treble;
}

int Spelling::getPitchClass() const
{
    return mod (naturalPitchClasses[(size_t) letter] + alter, 12);
}

juce::String Spelling::getName() const
{
    return letterNames[(size_t) letter] + getAccidentalText (alter);
}

int Pitch::getLetter() const   { return mod (step, 7); }
int Pitch::getOctave() const   { return floorDivide (step, 7); }

int Pitch::getMidiNoteNumber() const
{
    return 12 * (getOctave() + 1) + naturalPitchClasses[(size_t) getLetter()] + alter;
}

Spelling Pitch::getSpelling() const
{
    return { getLetter(), alter };
}

juce::String Pitch::getName() const
{
    return getSpelling().getName() + juce::String (getOctave());
}

//==============================================================================
const std::array<Spelling, 15>& getMajorKeys()
{
    static const std::array<Spelling, 15> keys
    {
        Spelling { 0, 0 }, Spelling { 0, 1 }, Spelling { 1, -1 }, Spelling { 1, 0 }, Spelling { 2, -1 },
        Spelling { 2, 0 }, Spelling { 3, 0 }, Spelling { 3, 1 }, Spelling { 4, -1 }, Spelling { 4, 0 },
        Spelling { 5, -1 }, Spelling { 5, 0 }, Spelling { 6, -1 }, Spelling { 6, 0 }, Spelling { 0, -1 },
    };

    return keys;
}

juce::String getKeyName (Spelling tonic)
{
    return tonic.getName() + " major";
}

Spelling getRelativeMinor (Spelling majorTonic)
{
    return spell (majorTonic.letter + 5, majorTonic.getPitchClass() + 9);
}

Spelling Key::getSignatureTonic() const
{
    return minor ? spell (tonic.letter + 2, tonic.getPitchClass() + 3) : tonic;
}

std::array<Spelling, 7> getScale (Key key)
{
    std::array<Spelling, 7> scale;
    const auto& steps = key.minor ? minorScaleSteps : majorScaleSteps;

    for (size_t degree = 0; degree < scale.size(); ++degree)
        scale[degree] = spell (key.tonic.letter + (int) degree, key.tonic.getPitchClass() + steps[degree]);

    return scale;
}

std::array<int, 7> getKeyAlterations (Key key)
{
    std::array<int, 7> alterations {};

    for (const auto& note : getScale (key))
        alterations[(size_t) note.letter] = note.alter;

    return alterations;
}

juce::String getKeySignatureText (Key key)
{
    std::vector<Spelling> altered;

    for (const auto& note : getScale (key))
        if (note.alter != 0)
            altered.push_back (note);

    if (altered.empty())
        return "No sharps or flats";

    const auto sharps = altered.front().alter > 0;
    const juce::String order (sharps ? "FCGDAEB" : "BEADGCF");

    std::sort (altered.begin(), altered.end(), [&] (const Spelling& a, const Spelling& b)
    {
        return order.indexOf (letterNames[(size_t) a.letter]) < order.indexOf (letterNames[(size_t) b.letter]);
    });

    juce::StringArray names;

    for (const auto& note : altered)
        names.add (note.getName());

    return juce::String ((int) altered.size()) + (sharps ? " sharp" : " flat") + (altered.size() > 1 ? "s" : "")
         + ": " + names.joinIntoString (" ");
}

Spelling spellPitchClass (Key key, int pitchClass)
{
    for (const auto& note : getScale (key))
        if (note.getPitchClass() == pitchClass)
            return note;

    // Outside the scale, flats in keys with flats, and F major's, sharps in the others
    const auto tonic = key.getSignatureTonic();

    if (const auto natural = std::find (naturalPitchClasses.begin(), naturalPitchClasses.end(), pitchClass);
        natural != naturalPitchClasses.end())
        return { (int) std::distance (naturalPitchClasses.begin(), natural), 0 };

    const auto shift = (tonic == Spelling { 3, 0 } || tonic.alter < 0) ? -1 : 1;
    const auto letter = std::find (naturalPitchClasses.begin(), naturalPitchClasses.end(), mod (pitchClass - shift, 12));
    return { (int) std::distance (naturalPitchClasses.begin(), letter), shift };
}

Pitch spellMidiNote (Key key, int midiNote)
{
    return makeTone ({ midiNote, spellPitchClass (key, mod (midiNote, 12)) }, {}).pitch;
}

//==============================================================================
const AddedNoteInfo& getInfo (AddedNote addedNote)
{
    static const std::array<AddedNoteInfo, 7> info
    {
        AddedNoteInfo { "",             0,  false, "",     "" },
        AddedNoteInfo { "Dominant 7th", 10, false, "7",    "7" },
        AddedNoteInfo { "Major 7th",    11, false, "maj7", "M7" },
        AddedNoteInfo { "Minor 7th",    10, false, "m7",   "7" },
        AddedNoteInfo { "Dominant 9th", 10, true,  "9",    "9" },
        AddedNoteInfo { "Major 9th",    11, true,  "maj9", "M9" },
        AddedNoteInfo { "Minor 9th",    10, true,  "m9",   "9" },
    };

    return info[(size_t) addedNote];
}

std::vector<AddedNote> getAddedNoteChoices (bool minor)
{
    if (minor)
        return { AddedNote::minor7th, AddedNote::minor9th };

    return { AddedNote::dominant7th, AddedNote::major7th, AddedNote::dominant9th, AddedNote::major9th };
}

juce::String getPositionName (int inversion)
{
    const char* const names[] { "Root position", "1st inversion", "2nd inversion", "3rd inversion" };
    return names[juce::jlimit (0, 3, inversion)];
}

juce::String Tone::getName() const
{
    return pitch.getSpelling().getName();
}

juce::String Tone::getNameWithOctave() const
{
    return pitch.getName();
}

Chord createChord (Key key, const ChordSpec& spec, Staff staff)
{
    jassert (! spec.isEmpty());

    const auto augmented = spec.altered && ! spec.minor;
    const auto diminished = spec.altered && spec.minor;
    const auto hasSeventh = spec.addedNote != AddedNote::none;
    const auto& added = getInfo (spec.addedNote);

    // Only 7th chords have a 3rd inversion.
    const auto inversion = hasSeventh ? spec.inversion : juce::jmin (spec.inversion, 2);

    const auto scaleRoot = getScale (key)[(size_t) spec.degree];
    const auto rootShift = spec.flat ? -1 : spec.sharp ? 1 : 0;
    const auto root = rootShift != 0 ? spell (scaleRoot.letter, naturalPitchClasses[(size_t) scaleRoot.letter] + scaleRoot.alter + rootShift)
                                     : scaleRoot;
    const auto keyAlterations = getKeyAlterations (key);

    std::vector<Member> members { { 0, 0 }, { 2, spec.minor ? 3 : 4 }, { 4, augmented ? 8 : diminished ? 6 : 7 } };

    if (hasSeventh)
        members.push_back ({ 6, added.seventhSemitones });

    if (added.hasNinth)
        members.push_back ({ 8, 14 });

    // An inversion moves the lowest members up an octave.
    std::vector<Member> voicing (members.begin() + inversion, members.end());

    for (auto i = 0; i < inversion; ++i)
        voicing.push_back ({ members[(size_t) i].steps + 7, members[(size_t) i].semitones + 12 });

    // Put the bass note in the clef's range, then move the chord by the spec's octave.
    const auto lowest = getLowestChordLetter (staff);
    const auto bassLetter = root.letter + voicing.front().steps;
    const auto shift = lowest + mod (bassLetter - lowest, 7) - bassLetter + 7 * spec.octave;

    Chord chord;

    for (const auto& member : voicing)
    {
        const auto step = root.letter + member.steps + shift;
        const auto spelling = spell (step, root.getPitchClass() + member.semitones);
        const Pitch pitch { step, spelling.alter };
        chord.tones.push_back ({ pitch, pitch.getMidiNoteNumber(), spelling.alter == keyAlterations[(size_t) spelling.letter] });
    }

    std::sort (chord.tones.begin(), chord.tones.end(), [] (const Tone& a, const Tone& b) { return a.midi < b.midi; });

    const auto roman = juce::String (degreeNumerals[(size_t) spec.degree]);

    chord.numeral = (spec.flat ? getSymbol (flat) : spec.sharp ? getSymbol (sharp) : juce::String()) + (spec.minor ? roman.toLowerCase() : roman);
    chord.sign = augmented ? "+" : diminished ? getSymbol (degreeSign) : juce::String();
    chord.seventhMark = spec.addedNote == AddedNote::major7th || spec.addedNote == AddedNote::major9th ? "M" : "";

    // A 9th chord shows 9 in root position; inverted, it takes the 7th chord's figures.
    chord.figures = added.hasNinth && inversion == 0 ? juce::StringArray { "9" }
                  : hasSeventh ? seventhFigures[(size_t) inversion]
                               : triadFigures[(size_t) inversion];

    chord.label = chord.numeral + chord.sign + added.numeral;
    chord.root = root;
    chord.inversion = inversion;

    const auto symbol = root.getName() + (hasSeventh ? juce::String (added.symbol)
                                                     : chord.sign.isNotEmpty() ? chord.sign
                                                     : spec.minor ? juce::String ("m") : juce::String());

    chord.symbol = spec.inversion != 0 ? symbol + "/" + chord.tones.front().getName() : symbol;

    const auto quality = hasSeventh ? juce::String (added.name).toLowerCase()
                       : augmented ? "augmented" : diminished ? "diminished" : spec.minor ? "minor" : "major";

    chord.name = root.getName() + " " + quality;
    return chord;
}

//==============================================================================
std::vector<Tone> createTones (Key key, const std::vector<KeyboardNote>& notes)
{
    const auto keyAlterations = getKeyAlterations (key);
    std::vector<Tone> tones;

    for (const auto& note : notes)
        tones.push_back (makeTone (note, keyAlterations));

    std::sort (tones.begin(), tones.end(), [] (const Tone& a, const Tone& b) { return a.midi < b.midi; });
    return tones;
}

Chord createChord (Key key, const std::vector<KeyboardNote>& notes, const ChordSpec& spec, Staff staff)
{
    const auto keyAlterations = getKeyAlterations (key);

    std::vector<Tone> tones;

    for (const auto& note : notes)
        tones.push_back (makeTone (note, keyAlterations));

    std::sort (tones.begin(), tones.end(), [] (const Tone& a, const Tone& b) { return a.midi < b.midi; });

    if (! spec.isEmpty())
    {
        // A 7th chord played without its 5th keeps its name, and its description says so.
        auto chord = createChord (key, spec, staff);

        ChordSpec rootPosition = spec;
        rootPosition.inversion = 0;
        rootPosition.octave = 0;

        const auto fifth = mod (createChord (key, rootPosition).tones[2].midi, 12);
        const auto lacksFifth = chord.tones.size() >= 4
                             && std::none_of (tones.begin(), tones.end(), [fifth] (const Tone& t) { return mod (t.midi, 12) == fifth; });

        chord.tones = tones;
        chord.fromKeyboard = true;
        chord.name += lacksFifth ? " (no 5th)" : "";
        return chord;
    }

    const auto found = identifyNotes (getMidiNotes (tones));
    const auto& bass = tones.front();
    using Kind = Identification::Kind;

    const auto findTone = [&] (int pitchClass)
    {
        return *std::find_if (tones.begin(), tones.end(), [pitchClass] (const Tone& t) { return mod (t.midi, 12) == pitchClass; });
    };

    Chord chord;
    chord.fromKeyboard = true;
    chord.tones = tones;
    chord.root = (found.kind == Kind::chord ? findTone (found.rootPitchClass) : bass).pitch.getSpelling();

    switch (found.kind)
    {
        case Kind::note:
            chord.symbol = chord.label = bass.getName();
            chord.name = bass.getNameWithOctave() + " (single note)";
            break;

        case Kind::octaves:
        {
            juce::StringArray names;

            for (const auto& tone : tones)
                names.add (tone.getNameWithOctave());

            chord.symbol = chord.label = "Octaves";
            chord.name = bass.getName() + " in octaves (" + names.joinIntoString (", ") + ")";
            break;
        }

        case Kind::interval:
        {
            // Two notes, or the lowest note and the first note of the other pitch class.
            const auto& upper = tones.size() == 2 ? tones[1]
                                                  : *std::find_if (tones.begin(), tones.end(), [&] (const Tone& t)
                                                                   {
                                                                       return mod (t.midi, 12) != mod (bass.midi, 12);
                                                                   });
            const auto interval = getIntervalName (bass, upper);

            chord.symbol = chord.label = interval;
            chord.name = interval + " (" + bass.getNameWithOctave() + getSymbol (enDash) + upper.getNameWithOctave() + ")";
            break;
        }

        case Kind::chord:
        {
            const auto root = findTone (found.rootPitchClass);

            chord.symbol = chord.label = root.getName() + found.shape->suffix + (found.inversion != 0 ? "/" + bass.getName() : juce::String());
            chord.name = root.getName() + " " + found.shape->name + (found.omitsFifth ? " (no 5th)" : "");
            break;
        }

        case Kind::unknown:
            chord.label = "?";
            chord.name = "Not a recognized chord";
            break;
    }

    return chord;
}

NotesDescription describeNotes (Key key, Staff staff, std::vector<int> midiNotes)
{
    std::sort (midiNotes.begin(), midiNotes.end());
    midiNotes.erase (std::unique (midiNotes.begin(), midiNotes.end()), midiNotes.end());

    NotesDescription description;

    if (midiNotes.empty())
        return description;

    const auto found = identifyNotes (midiNotes);
    std::function<Spelling (int)> spellingFor = [key] (int pitchClass) { return spellPitchClass (key, pitchClass); };

    if (found.kind == Identification::Kind::chord && found.shape->spec.has_value() && found.inversion <= 3)
    {
        // The panel can describe the chord: find its numeral, flattened if it isn't in the scale.
        const auto scale = getScale (key);
        const auto findDegree = [&] (int lowering)
        {
            const auto note = std::find_if (scale.begin(), scale.end(), [&] (const Spelling& s)
            {
                return mod (s.getPitchClass() - lowering, 12) == found.rootPitchClass;
            });

            return note == scale.end() ? -1 : (int) std::distance (scale.begin(), note);
        };

        // Outside the scale, a minor key's root on its raised 6th or 7th, as its melodic and
        // harmonic minor scales have them, is raised; any other is a lowered degree.
        auto& spec = description.spec;
        spec = *found.shape->spec;
        spec.degree = findDegree (0);

        if (spec.degree < 0)
        {
            const auto raised = findDegree (-1);

            if (key.minor && (raised == 5 || raised == 6))
            {
                spec.degree = raised;
                spec.sharp = true;
            }
            else
            {
                spec.degree = findDegree (1);
                spec.flat = true;
            }
        }

        spec.inversion = found.inversion;

        // If the notes are exactly what the panel would produce, the panel alone describes them.
        for (auto octave : { 0, -1, 1 })
        {
            spec.octave = octave;

            if (getMidiNotes (createChord (key, spec, staff).tones) == midiNotes)
                return description;
        }

        spec.octave = 0;

        ChordSpec rootPosition = spec;
        rootPosition.inversion = 0;
        const auto named = createChord (key, rootPosition);

        spellingFor = [named] (int pitchClass)
        {
            return std::find_if (named.tones.begin(), named.tones.end(), [pitchClass] (const Tone& t)
            {
                return t.pitch.getSpelling().getPitchClass() == pitchClass;
            })->pitch.getSpelling();
        };
    }
    else if (found.kind == Identification::Kind::chord)
    {
        // Spell the notes as the chord's members, counting letters up from its root.
        const auto root = spellPitchClass (key, found.rootPitchClass);
        const auto* shape = found.shape;
        const auto rootPitchClass = found.rootPitchClass;

        spellingFor = [root, shape, rootPitchClass] (int pitchClass)
        {
            const auto member = std::find_if (shape->members.begin(), shape->members.end(), [&] (const Member& m)
            {
                return mod (rootPitchClass + m.semitones, 12) == pitchClass;
            });

            return spell (root.letter + member->steps, pitchClass);
        };
    }

    std::vector<KeyboardNote> notes;

    for (auto midi : midiNotes)
        notes.push_back ({ midi, spellingFor (mod (midi, 12)) });

    description.keyboardNotes = notes;
    return description;
}

//==============================================================================
std::vector<Tone> getAlternateTones (Key key, const Chord& chord, Staff chordStaff, AlternateStaff alternate)
{
    if (alternate == AlternateStaff::none || chord.tones.empty())
        return {};

    const auto staff = getOtherStaff (chordStaff);
    const auto lowest = getLowestChordLetter (staff);
    const auto keyAlterations = getKeyAlterations (key);
    std::vector<Tone> tones;

    if (alternate == AlternateStaff::blockChord || alternate == AlternateStaff::rolledChord)
    {
        // The whole chord, moved by octaves so its lowest note sits in the other staff's range.
        const auto& bass = chord.tones.front();
        const auto octaves = (lowest + mod (bass.pitch.step - lowest, 7) - bass.pitch.step) / 7;

        for (const auto& tone : chord.tones)
            tones.push_back (makeTone ({ tone.midi + 12 * octaves, tone.pitch.getSpelling() }, keyAlterations));

        return tones;
    }

    // The root placed inside the other staff (F4-E5 treble, A2-G3 bass), or the root and the
    // octave above in that staff's low register (C4 and C5 up to B4 and B5 treble).
    const auto root = chord.root;
    const auto low = alternate == AlternateStaff::root ? getBottomLineLetter (staff) + 1 : lowest;
    const auto index = low + mod (root.letter - low, 7);

    const auto toneAt = [&] (int step)
    {
        return makeTone ({ 12 * (floorDivide (step, 7) + 1) + naturalPitchClasses[(size_t) root.letter] + root.alter, root }, keyAlterations);
    };

    tones.push_back (toneAt (index));

    if (alternate == AlternateStaff::octave)
        tones.push_back (toneAt (index + 7));

    return tones;
}

bool isMelodic (ChordType type)
{
    return type == ChordType::arpeggioUp || type == ChordType::arpeggioDown || type == ChordType::random;
}

int getArpeggioNotesPerBeat (int numNotes, int beatsPerMeasure)
{
    if (numNotes <= beatsPerMeasure)      return 1;
    if (numNotes <= beatsPerMeasure * 2)  return 2;
    return 4;
}

std::vector<int> createRandomOrder (const std::vector<int>& midiNotes, int numSlots, juce::Random& random)
{
    // Every note once, plus repeats taken in turn from a shuffled copy until the slots are full.
    auto shuffled = midiNotes;

    for (auto i = (int) shuffled.size() - 1; i > 0; --i)
        std::swap (shuffled[(size_t) i], shuffled[(size_t) random.nextInt (i + 1)]);

    std::vector<std::pair<int, int>> counts;    // note and how many times it's still to be used

    for (auto midi : midiNotes)
        counts.push_back ({ midi, 1 });

    for (auto k = (int) midiNotes.size(); k < numSlots; ++k)
    {
        const auto midi = shuffled[(size_t) (k - (int) midiNotes.size()) % shuffled.size()];
        std::find_if (counts.begin(), counts.end(), [midi] (const auto& c) { return c.first == midi; })->second++;
    }

    // Build the order a slot at a time, never repeating the previous note. A note that has to go
    // now or would end up repeating goes first.
    std::vector<int> order;

    for (auto left = numSlots; left > 0; --left)
    {
        std::vector<std::pair<int, int>> options;

        for (const auto& c : counts)
            if (c.second > 0 && (midiNotes.size() <= 1 || order.empty() || c.first != order.back()))
                options.push_back (c);

        std::vector<std::pair<int, int>> urgent;

        for (const auto& c : options)
            if (c.second > (left - 1) / 2.0)
                urgent.push_back (c);

        const auto& pool = urgent.empty() ? options : urgent;

        if (pool.empty())
            break;

        const auto pick = pool[(size_t) random.nextInt ((int) pool.size())].first;
        order.push_back (pick);
        std::find_if (counts.begin(), counts.end(), [pick] (const auto& c) { return c.first == pick; })->second--;
    }

    return order;
}

//==============================================================================
juce::String getDynamicMark (Dynamic dynamic)
{
    constexpr const char* marks[] { "ppp", "pp", "p", "mp", "mf", "f", "ff", "fff" };
    return marks[(size_t) dynamic];
}

juce::String getDynamicName (Dynamic dynamic)
{
    constexpr const char* names[] { "pianississimo", "pianissimo", "piano", "mezzo piano",
                                    "mezzo forte", "forte", "fortissimo", "fortississimo" };
    return names[(size_t) dynamic];
}

std::optional<Dynamic> findDynamic (const juce::String& mark)
{
    for (auto dynamic : allDynamics)
        if (getDynamicMark (dynamic) == mark)
            return dynamic;

    return {};
}

int getDynamicVelocity (Dynamic dynamic)
{
    const auto steps = (int) allDynamics.size() - 1;
    return softestVelocity + juce::roundToInt ((double) ((loudestVelocity - softestVelocity) * (int) dynamic) / steps);
}

juce::String getHairpinName (Hairpin hairpin)
{
    return hairpin == Hairpin::crescendo ? "Crescendo" : "Decrescendo";
}

double applyHairpin (Hairpin hairpin, double startVelocity, double beats)
{
    // The gap to the loudest or softest shrinks by the same fraction each beat.
    const auto towards = (double) (hairpin == Hairpin::crescendo ? loudestVelocity : softestVelocity);
    return towards - (towards - startVelocity) * std::pow (1.0 - hairpinChangePerBeat, beats);
}
}
