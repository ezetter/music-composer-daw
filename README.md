# Anthropocene Music

A native macOS music app written in C++ with [JUCE](https://juce.com).

It has two instruments, each on its own grand staff and played by its own VST3 plugin, that you can write notes and chords for, and an 88-key piano. Chords are defined the way the Chord Progression Builder defines them, one measure at a time. Sound goes to the Mac's default audio output.

## Building

You need macOS 12 or later, the Xcode Command Line Tools, and CMake 3.22 or later (`brew install cmake`).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
open "build/AnthropoceneMusic_artefacts/Debug/Anthropocene Music.app"
```

The first configure downloads JUCE 9.0.2 into `build/`.

## Using it

The window has a toolbar along the top, the score's settings on the left, the two instruments' staves, one above the other, and the piano under them.

### Instruments

The score has two instruments. They share the key, time signature, tempo and measures, and play together in time, but each has its own notes, chords, alternate staff and plugin. Instrument 1's staves are at the top and instrument 2's below; their measures line up, spaced to fit what's in either. Each instrument's name and plugin are written at its top left, e.g. "Instrument 1 · Pigments".

To the right of the last measure, between the two instruments' staves, **+** adds a measure at the end and **−** takes the last one away, for both instruments; **−** is disabled when there's only one. Adding a measure scrolls to the end, so **+** stays in view. Scroll sideways with a trackpad, the mouse wheel, or the scroll bar. Each instrument's staves take only the height their music needs: the chord buttons sit just over the chord names, which sit just over the highest notes, and the space above and below grows only when notes reach far beyond the staves.

One instrument is **active**, marked by a blue bar beside its staves. The piano and MIDI controllers play the active instrument, and the toolbar's plugin controls, the sidebar's **Alternate staff** and **Copy Progression** are the active instrument's. Choose it with **Instrument 1 / Instrument 2** in the toolbar, or click in an instrument's staves or its chord buttons. Keys held down when the other instrument becomes active are let go; the score's notes play on.

Each instrument has a **volume dial** to the left of its staves, setting how loud it is in the mix, from **Off** up to +6 dB. It starts at 0 dB, which leaves the plugin as loud as it is. Drag up or down, or round, to turn it. **Click** it without turning it to mute the instrument, and click again to unmute it; a muted dial is grey and says **Muted**, and unmuting brings back the volume it had. Turning a muted dial unmutes it. The dials follow the staves as they scroll up and down. Volumes change smoothly, without clicks, even while the score plays. A volume change, or muting, counts as a change to the score, which saves each instrument's volume and whether it's muted; they're also remembered the next time the app starts, and stay as they are for a new score.

### Toolbar

- **Play** (or the space bar) plays the score from the beginning, highlighting each beat and following it along the staff. **Stop** stops it.
- **Loop**, beside Play, makes the score play over and over, going straight from the end back to the beginning, until you press **Stop**. It can be turned on or off while the score plays; turning it off lets the score finish this time through. While it loops, changes to the score, tempo included, are heard from the next time through. Loop is remembered the next time the app starts.
- **BPM** sets the tempo in quarter notes per minute, from 20 to 300, fractions allowed. The default is 120. A change applies from the next Play, or the next time through a loop.
- **Instrument 1 / Instrument 2** chooses the active instrument.
- **Load Instrument…** loads a VST3 instrument plugin for the active instrument, such as one from `/Library/Audio/Plug-Ins/VST3`. **Edit [plugin]**, named for the loaded plugin (e.g. **Edit Pigments**), opens the plugin's own window, where you can choose its sounds. The two instruments can use different plugins, or the same one with different sounds.
- Each instrument's plugin is saved with the score (see [Saving and opening](#saving-and-opening)). It's also loaded again the next time the app starts, with the sound it had. It's saved when you load it, when you close its editor, and when you quit, in `~/Library/Application Support/Anthropocene Music`. If it can't be loaded, say because it's been uninstalled, the app tells you and forgets it.

### Saving and opening

The **File** menu saves and opens scores:

| Command | Shortcut | What it does |
| --- | --- | --- |
| **New** | ⌘N | Starts an empty score: four measures of 4/4 in C major at 120 BPM. |
| **Open…** | ⌘O | Opens a saved score. |
| **Open Recent** | | Lists the 10 scores opened or saved most recently, newest first, and opens the one you choose. Scores that have been moved or deleted aren't listed, and two with the same name say which folder they're in. **Clear Menu** empties the list. The list is remembered from one run of the app to the next, and the scores also appear in the Mac's own recent documents. |
| **Save** | ⌘S | Saves the score to its file, asking for a name the first time. |
| **Save As…** | ⇧⌘S | Saves the score to a new file, which it then keeps using. |

- Scores are saved as `.amscore` files, which are JSON. A file keeps everything about the score, and the plugins it's played on and their volumes: the key, time signature, tempo, and for each instrument its alternate staff, its measures and their notes, and each measure's chord with all its settings. That includes notes you've added to or taken out of a chord, and the order a Random chord plays in. Notes on beats hidden by a shorter time signature are kept too.
- Each instrument's plugin is saved with the score, with its sound: whatever you've chosen in its editor. Opening the score loads the plugins again, or just gives it the saved sound if it's the one already loaded. A plugin's sound can be large, so a score can be too; Pigments adds about half a megabyte.
- Loading a different instrument, or changing its sound in its editor, counts as a change that needs saving, and the title shows it within half a second. It goes by what the instrument reports: a change to one of its parameters, its program, or its other settings. Just opening the editor, or playing, isn't a change, even though some instruments (Pigments, for one) save details of their editor window along with their sound.
- If a score's plugin can't be loaded, say because it isn't installed, the score still opens, you're told, and the plugin you had stays loaded. An instrument saved without a plugin also leaves the one it has as it is, and so does New. Scores saved before there were two instruments open as instrument 1, with their plugin, and an empty instrument 2.
- The window's title shows the score's name, and "Edited" when it has changes that haven't been saved. Before New, Open, or quitting (including closing the window), you're asked whether to save them.
- If a file can't be opened, because it isn't a score, you're told why and the current score stays as it was. A score with values out of range, say from editing the file by hand, opens with those values set back to their defaults.
- A newly opened or new score starts scrolled to the beginning, with the chord window closed.

### Score

| Setting | What it does |
| --- | --- |
| **Key** | The 15 major keys, C♭ to C♯. Sets the key signature, what the chord window's numerals mean, and the sharps and flats of notes you click in. Chords follow their numerals into a new key; notes keep their pitches and get accidentals if they need them. |
| **Time signature** | 2/4, 3/4 or 4/4. Notes on beats that no longer fit are kept, and come back if you go back to more beats. |
| **Alternate staff** | For the active instrument, what every chord puts on the other staff: **None**, the chord's **Root**, the root in an **Octave**, or the whole chord as a **Block chord** or **Rolled chord**, held for the measure. It applies to all the instrument's chords, on whichever staff each one isn't; each instrument has its own. Quarter notes under or over a chord are hidden while there's an alternate staff, rather than lost, and come back with None. Changing it puts any alternate-staff notes you've changed by hand back to what the chords give. Scores saved before this was one setting open with the alternate staff most of their chords had. |
| **Progression** | **Copy Progression to Instrument 2** (or **to Instrument 1**, when instrument 2 is active) gives the other instrument the active instrument's chords, measure by measure, in the same clefs, chord types and Random orders. It replaces what the other instrument had: its chords, and its quarter notes, hidden ones included; measures without a chord come over empty. The other instrument keeps its own alternate staff. If the other instrument has any notes or chords, you're asked first, with **Replace** or **Cancel**; if it's empty, the progression is copied straight away. The button is disabled while the active instrument has no chords. |

### Notes

Clicking the staff adds a quarter note at that pitch and beat, taking its sharp or flat from the key signature. A blue note shows where it will go. Clicking between the staves puts the note on the nearer staff, with ledger lines. Rests fill the beats without notes.

Clicking a quarter note takes it out again, whatever its sharp or flat. The note under the pointer turns red to show that a click will remove it.

On a staff with a chord's notes, clicking works the same way on the chord:
- **Removing:** clicking one of the chord's notes takes it out of the chord.
- **Adding:** clicking anywhere else on the staff adds that note to the chord, and the chord's other notes stay.
- **Both staves:** this works on the chord's own staff and on its alternate staff.
- **Renaming:** a chord changed this way is renamed for its new notes, as with notes set on the piano.
- **Resetting:** changing the chord in the chord window gives the alternate staff the chord's own notes back.
- **Empty chord:** taking out all the chord's notes frees the staff for quarter notes again.

### Chords

Above each measure of each instrument is an **Add Chord** button, or **Edit Chord** if the measure has a chord. It makes the instrument active and opens the chord window for that measure, and outlines the measure while the window is open. Making the other instrument active closes it. For a chord that's already there, the window starts with its settings.

Everything you change in the window goes straight into the measure, and the staff updates as you go. **Remove Chord** empties the measure, leaving the window open for a new chord. **Done**, Return, Esc or the window's close button closes it. Clicking another measure's button moves the window on to that measure. The window floats over the main window, so the staff and piano can still be used while it's open, and changes made to the chord on the staff show in the window too.

The chord window has the Chord Progression Builder's settings for one chord:

1. **♭ Flat** lowers the chord's root a half step, so VII becomes ♭VII.
2. **Major / Minor** chooses the triad, and **+ / °** makes a major chord augmented or a minor chord diminished.
3. **Numeral** is the scale step. Each option names the chord in the current key, e.g. "vi (Am)". Choose "—" for no chord.
4. **Added note** makes a 7th or 9th chord. It can't be on with + / °; choosing one clears the other.
5. **Position** is root position or the 1st, 2nd or 3rd inversion. The 3rd is only for 7th and 9th chords.
6. **Octave** moves the chord down or up an octave.
7. **Treble clef / Bass clef** is the staff the chord is written on. Its lowest note falls in C4–B4 on the treble staff and E2–D3 on the bass.
8. **Chord type**:
   - **Block**: a chord held for the whole measure, written at its start so it lines up with a quarter note on the first beat of the other staff.
   - **Arpeggio (asc)** and **Arpeggio (desc)**: single notes, low to high or high to low, padded with rests. Quarter notes are used if they fit, otherwise eighths or sixteenths, beamed a beat at a time.
   - **Random**: the notes in a random order, filling every beat without playing any note twice in a row. **Reshuffle** picks a new order.
   - **Rolled chord**: a block chord with the wavy roll sign.

Each chord keeps its own clef and chord type. A new chord starts with the ones chosen last. A chord replaces any quarter notes on its staff.

Under the settings, the window lists the chord's name and position, its notes from low to high, and whether they're in the key.

**On the piano:** while the chord window is open, clicking a key adds its note to the window's chord, or takes it out. The chord's notes are marked on the keys. The notes are named as a note, an interval, or a chord (including sus2, sus4, ø7, °7 and m(maj7)), and spelled to suit it, e.g. A♭ rather than G♯ in B°7. When the window can describe the notes, it fills in to match. Notes set on the piano keep their pitches if you change the key.

### What the staff shows

- Chord names above each chord, e.g. Am, G7, B♭maj9, B°, E+, and slash chords for inversions such as G/B.
- Roman numerals below, with their figures: I⁶, I⁶₄, V⁷, V⁶₅, V⁴₃, V⁴₂, V⁹, and an M before a major 7th's figures (I^M7).
- Accidentals only where a note differs from the key signature or from an earlier accidental in the measure.

### Playing

Block chords sound together and rolled chords bottom to top in quick succession. Arpeggio notes come one per written note and ring on to the end of the measure. Alternate-staff notes start on the downbeat. Quarter notes play for their beat.

Click or drag across the piano keys to play the active instrument yourself. Once the piano has been clicked, the computer keyboard plays it too: A is middle C, and the row from A to L (with W, E, T, Y, U, O and P for the black keys) plays upwards from there.

### MIDI controllers

Plug a MIDI controller into a USB port and it plays the active instrument like the on-screen piano, whose keys go down as you play. It's used as soon as it's plugged in, even with the app already running. The keys play at the velocity you strike them, and the sustain pedal, pitch bend, mod wheel and other controls go through to the plugin too. While the chord window is open, a key adds its note to the window's chord or takes it out, as clicking the piano does.

The **MIDI** menu lists the MIDI inputs, ticked when they're on. Choose one to turn it off or on; one that's off stays off the next time the app starts.

## Source

| File | Contents |
| --- | --- |
| `Source/Main.cpp` | The application and its window, and asking to save before quitting |
| `Source/MainComponent.*` | The File and MIDI menus, the toolbar, the sidebar, the two instruments' scrolling staves and their volume dials, the piano, and the audio output |
| `Source/Score.*` | The music: the key, time signature and speed, and each instrument's quarter notes, chords and alternate staff (a part, in the code), and saving it as JSON |
| `Source/ScoreDocument.*` | Saves the score to a file and opens it, and keeps track of unsaved changes |
| `Source/Music.*` | Music theory: keys, spelling, and building, recognising and naming chords |
| `Source/MeasureContent.*` | Works out the notes, rests and their lengths that each measure has on each staff |
| `Source/StaffView.*` | Draws an instrument's grand staff with each measure's chord button, lined up with the other instrument's, and turns clicks into notes |
| `Source/MusicGlyphs.*` | Music symbols from the Bravura font |
| `Source/ScorePanel.*` | The key, time signature and measures settings |
| `Source/ChordEditor.*` | The chord window, with the settings for a measure's chord |
| `Source/PianoKeyboard.*` | The on-screen piano |
| `Source/Controls.h` | Colours and helpers shared by the controls |
| `Source/InstrumentHost.*` | Hosts each instrument's plugin, and plays them from the piano, MIDI controllers and the score, once or looping |
| `Source/MidiInputs.*` | Connects MIDI controllers, as they're plugged in, and the MIDI menu's settings |
| `Source/InstrumentPanel.*` | Loads an instrument's plugin and opens its editor, and saves and loads it with the score |
| `Source/PluginWindow.*` | The window for an instrument's editor |
| `Resources/Icon.png` | The app's icon, a staff with two quarter notes, from which the build makes the Mac icon |

## Licences

JUCE is dual-licensed under the AGPLv3 and the commercial JUCE licence. The Bravura music font is © Steinberg Media Technologies GmbH and licensed under the SIL Open Font License 1.1; see `Resources/Fonts/Bravura-LICENSE.txt`.
