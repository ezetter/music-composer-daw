# Anthropocene Music

A native macOS music app written in C++ with [JUCE](https://juce.com).

It has a grand staff you can write notes and chords on, an 88-key piano, and a VST3 instrument that plays them. Chords are defined the way the Chord Progression Builder defines them, one measure at a time. Sound goes to the Mac's default audio output.

## Building

You need macOS 12 or later, the Xcode Command Line Tools, and CMake 3.22 or later (`brew install cmake`).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
open "build/AnthropoceneMusic_artefacts/Debug/Anthropocene Music.app"
```

The first configure downloads JUCE 9.0.2 into `build/`.

## Using it

The window has a toolbar along the top, the score's and chords' settings on the left, the staff, and the piano under it.

### Toolbar

- **Play** (or the space bar) plays the score from the beginning, highlighting each beat and following it along the staff. **Stop** stops it.
- **BPM** sets the tempo in quarter notes per minute, from 20 to 300, fractions allowed. The default is 120. A change applies from the next Play.
- **Notes / Chords** chooses what clicking the staff does. See below.
- **Load Instrument…** loads a VST3 instrument, such as one from `/Library/Audio/Plug-Ins/VST3`. **Show Editor** opens the instrument's own window, where you can choose its sounds.
- The instrument is loaded again the next time the app starts, with the sound it had. It's saved when you load it, when you close its editor, and when you quit, in `~/Library/Application Support/Anthropocene Music`. If it can't be loaded, say because it's been uninstalled, the app tells you and forgets it.

### Score

| Setting | What it does |
| --- | --- |
| **Key** | The 15 major keys, C♭ to C♯. Sets the key signature, what the chord panel's numerals mean, and the sharps and flats of notes you click in. Chords follow their numerals into a new key; notes keep their pitches and get accidentals if they need them. |
| **Time signature** | 2/4, 3/4 or 4/4. Notes on beats that no longer fit are kept, and come back if you go back to more beats. |
| **Measures** | **Add Measure** and **Remove Measure** change the length of the score. Scroll sideways with a trackpad, the mouse wheel, or the scroll bar. |

### Notes

In **Notes** mode, clicking the staff adds a quarter note at that pitch and beat, taking its sharp or flat from the key signature. A blue note shows where it will go. Clicking between the staves puts the note on the nearer staff, with ledger lines. Rests fill the beats without notes.

Clicking a quarter note takes it out again, whatever its sharp or flat. The note under the pointer turns red to show that a click will remove it. The notes of a measure's chord are changed in Chords mode instead.

### Chords

In **Chords** mode, clicking a measure selects it and plays it, and the chord panel edits that measure's chord. Clicking it again, or pressing Esc, lets it go. Everything updates as you change it.

Switching to Chords selects the first measure without any notes, ready for a chord, or the first measure if they all have notes. It scrolls that measure into view.

The chord panel has the Chord Progression Builder's settings for one chord:

1. **♭ Flat** lowers the chord's root a half step, so VII becomes ♭VII.
2. **Major / Minor** chooses the triad, and **+ / °** makes a major chord augmented or a minor chord diminished.
3. **Numeral** is the scale step. Each option names the chord in the current key, e.g. "vi (Am)". Choose "—" for no chord.
4. **Added note** makes a 7th or 9th chord. It can't be on with + / °; choosing one clears the other.
5. **Position** is root position or the 1st, 2nd or 3rd inversion. The 3rd is only for 7th and 9th chords.
6. **Octave** moves the chord down or up an octave.
7. **Treble clef / Bass clef** is the staff the chord is written on. Its lowest note falls in C4–B4 on the treble staff and E2–D3 on the bass.
8. **Chord type**:
   - **Block**: a chord filling the measure.
   - **Arpeggio (asc)** and **Arpeggio (desc)**: single notes, low to high or high to low, padded with rests. Quarter notes are used if they fit, otherwise eighths or sixteenths, beamed a beat at a time.
   - **Random**: the notes in a random order, filling every beat without playing any note twice in a row. **Reshuffle** picks a new order.
   - **Rolled chord**: a block chord with the wavy roll sign.
9. **Alternate staff** puts notes on the other staff: the chord's **Root**, the root in an **Octave**, or the whole chord as a **Block chord** or **Rolled chord**, held for the measure.

Each chord keeps its own clef, chord type and alternate staff. A measure's first chord starts with the ones chosen last. A chord replaces any quarter notes on the staves it uses, and **Clear Chord** empties the measure.

Under the settings, the panel lists the chord's name and position, its notes from low to high, and whether they're in the key.

**On the piano:** with a measure selected, clicking a key adds its note to the chord, or takes it out. The chord's notes are marked on the keys. The notes are named as a note, an interval, or a chord (including sus2, sus4, ø7, °7 and m(maj7)), and spelled to suit it, e.g. A♭ rather than G♯ in B°7. When the panel can describe the notes, it fills in to match. Notes set on the piano keep their pitches if you change the key.

### What the staff shows

- Chord names above each chord, e.g. Am, G7, B♭maj9, B°, E+, and slash chords for inversions such as G/B.
- Roman numerals below, with their figures: I⁶, I⁶₄, V⁷, V⁶₅, V⁴₃, V⁴₂, V⁹, and an M before a major 7th's figures (I^M7).
- Accidentals only where a note differs from the key signature or from an earlier accidental in the measure.

### Playing

Block chords sound together and rolled chords bottom to top in quick succession. Arpeggio notes come one per written note and ring on to the end of the measure. Alternate-staff notes start on the downbeat. Quarter notes play for their beat.

Click or drag across the piano keys to play the instrument yourself. Once the piano has been clicked, the computer keyboard plays it too: A is middle C, and the row from A to L (with W, E, T, Y, U, O and P for the black keys) plays upwards from there.

## Source

| File | Contents |
| --- | --- |
| `Source/Main.cpp` | The application and its window |
| `Source/MainComponent.*` | The toolbar, the sidebar, the scrolling staff, the piano, and the audio output |
| `Source/Score.*` | The music: the key, time signature and speed, quarter notes, and each measure's chord |
| `Source/Music.*` | Music theory: keys, spelling, and building, recognising and naming chords |
| `Source/MeasureContent.*` | Works out the notes, rests and their lengths that each measure has on each staff |
| `Source/StaffView.*` | Draws the grand staff, and turns clicks into notes or selected measures |
| `Source/MusicGlyphs.*` | Music symbols from the Bravura font |
| `Source/ScorePanel.*` | The key, time signature and measures settings |
| `Source/ChordPanel.*` | The settings for a measure's chord |
| `Source/PianoKeyboard.*` | The on-screen piano |
| `Source/Controls.h` | Colours and helpers shared by the controls |
| `Source/InstrumentHost.*` | Hosts the instrument plugin, and plays it from the piano and the score |
| `Source/InstrumentPanel.*` | Loads instruments and opens their editors |
| `Source/PluginWindow.*` | The window for an instrument's editor |

## Licences

JUCE is dual-licensed under the AGPLv3 and the commercial JUCE licence. The Bravura music font is © Steinberg Media Technologies GmbH and licensed under the SIL Open Font License 1.1; see `Resources/Fonts/Bravura-LICENSE.txt`.
