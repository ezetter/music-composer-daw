# Anthropocene Music

A native macOS music app written in C++ with [JUCE](https://juce.com).

It has as many instruments as you like, two to start with, each on its own grand staff and played by its own VST3 plugin, that you can write notes and chords for, and an 88-key piano. Chords are defined the way the Chord Progression Builder defines them, one measure at a time. Sound goes to the Mac's default audio output.

## Building

You need macOS 12 or later, the Xcode Command Line Tools, and CMake 3.22 or later (`brew install cmake`).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
open "build/AnthropoceneMusic_artefacts/Debug/Anthropocene Music.app"
```

The first configure downloads JUCE 9.0.2 into `build/`.

## Using it

The window has a toolbar along the top, the score's settings on the left, the instruments' staves, one above the other, and the piano under them.

### Instruments

The score has two instruments to start with, and can have any number. They share the key, time signature, tempo and measures, and play together in time, but each has its own notes, chords, dynamics, alternate staff and plugin. Instrument 1's staves are at the top and the others' below, in order; their measures line up, spaced to fit what's in any of them. Each instrument's name and plugin are written at its top left, e.g. "Instrument 1 · Pigments".

**Adding and deleting instruments.** The **+** below the lowest instrument's staves, marked **Add instrument**, adds one below it, with its own staves, volume dial and plugin. It starts empty, without a plugin, at 0 dB, becomes the active instrument, and is scrolled into view. With more instruments than fit, scroll up and down through them with a trackpad, the mouse wheel or the scroll bar; the volume dials scroll with them. Under each instrument's volume dial is a **bin** button that deletes the instrument, with its staves; the dial and the bin sit together beside the middle of the staves. An instrument with nothing in it and no plugin goes straight away. Otherwise you're asked first, with **Delete** or **Cancel**, since its notes, chords and dynamics go with it and its plugin is unloaded. The instruments after it move up, with their music, plugins and volumes, and are numbered again. The last instrument can't be deleted. Adding and deleting instruments can be undone; undoing a delete puts the music back in its place, but the plugin needs loading again. New starts again with two instruments, the first two keeping their plugins; opening a score gives it as many instruments as it was saved with.

Just after the final barline, halfway down the staves in view, **+** and **−**, side by side, add a measure at the end and take the last one away, for every instrument; **−** is greyed out unless the last measure shows nothing but rests in every instrument (no notes and no chord, though dynamics and hairpins don't count), and when there's only one measure; its tooltip says why. **Clone**, under them, repeats all the measures after the last one, for every instrument: 4 measures become 8, the second 4 a copy of the first, with all their notes and chords, to be edited on their own. They scroll sideways with the staves, so they're in view when the end of the score is, but stay halfway down what's in view as you scroll up and down through the instruments. The staves reach all the way to the right edge of the window. Adding or cloning measures scrolls to the end, so the new measures are in view. Scroll sideways with a trackpad, the mouse wheel, or the scroll bar. Each instrument's staves take only the height their music needs: the chord buttons sit just over the chord names, which sit just over the highest notes, and the space above and below grows only when notes reach far beyond the staves.

One instrument is **active**, marked by a blue bar beside its staves. The piano and MIDI controllers play the active instrument, and the toolbar's plugin controls, the sidebar's **Alternate staff** and **Copy Progression** are the active instrument's. Choose it from the instrument menu in the toolbar, or click in an instrument's staves or its chord buttons. Keys held down when another instrument becomes active are let go; the score's notes play on.

Each instrument has a **volume dial** to the left of its staves, setting how loud it is in the mix, from **Off** up to +6 dB. It starts at 0 dB, which leaves the plugin as loud as it is. Drag up or down, or round, to turn it. **Click** it without turning it to mute the instrument, and click again to unmute it; a muted dial is grey and says **Muted**, and unmuting brings back the volume it had. Turning a muted dial unmutes it. The dials follow the staves as they scroll up and down. Volumes change smoothly, without clicks, even while the score plays. A volume change, or muting, counts as a change to the score, which saves each instrument's volume and whether it's muted; they're also remembered the next time the app starts, and stay as they are for a new score.

### Toolbar

- **Play** (or the space bar) plays the score from the beginning, highlighting each beat and following it along the staff. **Stop** stops it.
- **Loop**, beside Play, makes the score play over and over, going straight from the end back to the beginning, until you press **Stop**. It can be turned on or off while the score plays; turning it off lets the score finish this time through. While it loops, changes to the score, tempo included, are heard from the next time through. Loop is remembered the next time the app starts.
- **BPM** sets the tempo in quarter notes per minute, from 20 to 300, fractions allowed. The default is 120. A change applies from the next Play, or the next time through a loop.
- The instrument menu (**Instrument 1**, **Instrument 2** and so on) chooses the active instrument.
- **Load Instrument…** loads a VST3 instrument plugin for the active instrument, such as one from `/Library/Audio/Plug-Ins/VST3`. **Edit [plugin]**, named for the loaded plugin (e.g. **Edit Pigments**), opens the plugin's own window, where you can choose its sounds. The instruments can use different plugins, or the same one with different sounds.
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

### Undo

The **Edit** menu's **Undo** (⌘Z) undoes the last change to the score, and pressing it again undoes the one before, back to how the score was when it was opened or started (the last 500 changes are remembered). **Redo** (⇧⌘Z) makes an undone change again, until something else is changed. Each is greyed out when there's nothing to undo or redo.

- Everything about the score can be undone: notes, ties, chords and the chord window's changes, dynamics and hairpins, the key, time signature, tempo and alternate staff, and adding, removing, cloning and copying measures and progressions. What one click, drag or command does is undone in one go, e.g. a whole Copy Progression, a hairpin however far it was dragged, or everything an eraser stroke took out.
- The chord window shows the chord as undoing leaves it, and closes if its measure's undone away.
- Undoing counts as a change that needs saving. The instruments, their volumes and muting, and what's chosen to add, such as the note length, aren't part of what's undone.
- Typing in the BPM box, ⌘Z undoes the typing instead.

### Score

| Setting | What it does |
| --- | --- |
| **Key** | The 15 major keys, C♭ to C♯. Sets the key signature, what the chord window's numerals mean, and the sharps and flats of notes you click in. Chords follow their numerals into a new key; notes keep their pitches and get accidentals if they need them. |
| **Time signature** | 2/4, 3/4 or 4/4. Notes on beats that no longer fit are kept, and come back if you go back to more beats. |
| **Alternate staff** | For the active instrument, what every chord puts on the other staff: **None**, the chord's **Root**, the root in an **Octave**, or the whole chord as a **Block chord** or **Rolled chord**, held for the measure. It applies to all the instrument's chords, on whichever staff each one isn't; each instrument has its own. Quarter notes under or over a chord are hidden while there's an alternate staff, rather than lost, and come back with None. Changing it puts any alternate-staff notes you've changed by hand back to what the chords give. Scores saved before this was one setting open with the alternate staff most of their chords had. |
| **Progression** | **Copy Progression to Instrument 2** (or **to Instrument 1**, when instrument 2 is active) gives the other instrument the active instrument's chords. With more than two instruments it's **Copy Progression To…**, which asks which instrument to copy to; with only one, there's nowhere to copy to, and it's disabled. The progression goes over measure by measure, in the same clefs, chord types and Random orders. It replaces what the other instrument had: its chords, and its quarter notes, hidden ones included; measures without a chord come over empty. The other instrument keeps its own alternate staff. If the other instrument has any notes or chords, you're asked first, with **Replace** or **Cancel**; if it's empty, the progression is copied straight away. The button is disabled while the active instrument has no chords. |

### Notes

Clicking the staff adds a note at that pitch and beat, taking its sharp or flat from the key signature. A blue note shows where it will go. Clicking between the staves puts the note on the nearer staff, with ledger lines. Rests fill the beats without notes.

Nine small buttons in the top left corner of the score, showing a whole, a dotted half, a half, a dotted quarter, a quarter, a dotted eighth, an eighth, a 16th and a 32nd note, choose the length of the notes that clicks add. They stay in the corner as the staves scroll. Quarter notes are chosen each time the app opens, and the blue note shows the length chosen. A note is as long as there's room for in its measure: a half note on the last beat of 4/4 is a quarter note, a dotted half on the third beat is a half note, a dotted quarter on the last beat is a quarter, and a whole note in 3/4 is a dotted half. A longer note takes the place of any notes on the beats it covers, and adding a note during a longer one cuts the longer one short. Notes starting on the same beat share a length, so a note added to them gives them its own. A shorter time signature shows notes shorter if it has to, and they're as long as they were again in 4/4. Notes shorter than a beat go on the nearest step of their own length without its dot: eighths and dotted eighths on the beats and halfway between them, 16ths on every 16th, and 32nds on every 32nd. Dotted notes are written with a dot in a space beside the notehead, past the flag of a flagged note with its stem up; a dotted eighth and a 16th that follows it share a beam. The other lengths go on the nearest beat, though a note already between beats can still be clicked to take it out. Four eighths in a row, each starting as the one before ends, share one beam (eight in a row are two groups of four); otherwise eighths, 16ths and 32nds are beamed together a beat at a time, with a second beam joining the 16ths and 32nds and a third the 32nds, and a short beam for one with no neighbour to share it. So three eighths in a row are a beamed pair and a flagged eighth, and a lone eighth, 16th or 32nd has its flags. Parts of beats left empty get the eighth, 16th and 32nd rests that fill them, in step with the beat. Notes play for as long as they're written, and their lengths are saved with the score; scores saved before there were note lengths open with quarter notes, and scores saved before eighth notes, or before 16ths and 32nds, open with their notes on the same beats as before.

Clicking a note takes it out again, whatever its sharp or flat. The note under the pointer turns red to show that a click will remove it. Notes are added and taken out when the mouse is let go, as long as it hasn't moved.

**Ties.** Drag from a note to the next note of the same pitch, the one that starts just as it ends, and let go: they're tied, with the usual curved tie between the noteheads, and play as one note held for both their lengths. A tie can cross a barline, so a note can last longer than a measure, and ties can be chained. A blue tie follows the mouse as you drag, and turns solid over a note it can tie to. Dragging between two tied notes again unties them. Letting go anywhere else, over a note of another pitch, over an empty spot, or over a same-pitched note that doesn't follow on, does nothing. A single note's tie curves away from its stem; in notes stacked on one beat, the upper ties curve up and the lower down. Taking out a tied note, or writing a longer note over it, takes its tie away too. Ties are saved with the score, and Clone copies them. A chord's notes don't tie.

On a staff with a chord's notes, clicking works the same way on the chord:
- **Removing:** clicking one of the chord's notes takes it out of the chord.
- **Random chords:** a Random chord's notes are taken out one at a time instead, and only the note under the pointer turns red. Clicking it, or erasing it, takes the chord out of the measure: its other notes, on both staves, stay exactly where and as long as they were, as notes of their own, as if they'd been clicked in one by one, and the measure's button says **Add Chord** again. Undo puts the chord back. Block, rolled and arpeggio chords keep their chords, with the note taken out of them.
- **Adding:** clicking anywhere else on the staff adds that note to the chord, and the chord's other notes stay.
- **Both staves:** this works on the chord's own staff and on its alternate staff.
- **Renaming:** a chord changed this way is renamed for its new notes, as with notes set on the piano.
- **Resetting:** changing the chord in the chord window gives the alternate staff the chord's own notes back.
- **Empty chord:** taking out all the chord's notes frees the staff for quarter notes again.

### Dynamics

Eight buttons beside the note lengths, showing **ppp**, **pp**, **p**, **mp**, **mf**, **f**, **ff** and **fff**, choose a dynamic for clicks to mark instead of adding notes. While one is chosen, no note length shows as chosen, and clicking either instrument's staves marks the dynamic between its staves, at the nearest beat, or halfway between beats where a note starts there on either staff. A faint copy shows where it will go. Clicking a different dynamic's mark replaces it, and clicking the same one's takes it out. Clicking the chosen dynamic's button again, or any note length, goes back to adding notes.

Each instrument has its own dynamics. A dynamic sets how hard the instrument's notes are played, as MIDI velocity, from where it's marked until the next one, across barlines: ppp 20, pp 35, p 51, mp 66, mf 81, f 96, ff 112 and fff 127. Notes before the first dynamic play at 102, as every note did before there were dynamics. Playback that starts part way through the score uses the dynamic marked before that point. A tied note keeps the velocity it started with. Clone copies dynamics with the measures, and Copy Progression leaves the other instrument's dynamics as they are. A dynamic on a beat a shorter time signature leaves out is kept but doesn't show or play, as with notes. Dynamics are saved with the score.

**Crescendos and decrescendos.** After the dynamics are a crescendo button (<) and a decrescendo button (>). With one chosen, drag along an instrument's staves to mark it: it's drawn between the staves as a hairpin, a wedge opening out for a crescendo or closing for a decrescendo, and stretches with the mouse as you drag, from the beat where the mouse went down to the beat nearest where it's let go (or a barline, or halfway through a beat where a note starts). A click without dragging marks one a beat long. Drag the end of a hairpin to stretch or shrink it; the pointer becomes a left-right arrow over it. Clicking a hairpin takes it out. Hairpins can run across barlines, but not past the end of the score, and one marked over others takes their place. A hairpin starts after a dynamic marked where it starts, and ends before one where it ends.

Through a hairpin, each beat takes the velocity 10% of the way from where it is to the loudest, 127, for a crescendo, or to the softest, 20, for a decrescendo, so a crescendo from pp grows faster than one from ff: from pp (35) a beat later it's 44, and from ff (112) 114. Notes part way through a beat are part of the way there. After the hairpin, the velocity stays where it left it, until the next dynamic. A dynamic in the middle of a hairpin sets the velocity, and the hairpin carries on from there. Hairpins are saved with the score, and Clone copies them with the measures.

### Eraser

The eraser button, floating over the top right of the staves, always in view and apart from the note lengths, dynamics and hairpins, turns the eraser on, and the pointer becomes an eraser. Hold the mouse button down and move over the staves, and whatever the eraser passes over is taken out: notes, by their noteheads; a chord's notes, which come out of the chord, as clicking them does; ties, letting go of the notes they join; dynamics; and hairpins, anywhere along them. It catches everything along the way, however quickly it's moved. A click without moving takes out what's under the pointer. Before you press, whatever the eraser is over is shown in red. It works on either instrument's staves, and never adds anything.

Everything one stroke takes out is undone in one go. Clicking the eraser button again, or choosing a note length, dynamic or hairpin, goes back to adding those. Chord names and numerals aren't erased; take a chord out with Remove Chord in its window, or by erasing its notes.

### Chords

Above each measure of each instrument is an **Add Chord** button, or **Edit Chord** if the measure has a chord. It makes the instrument active and opens the chord window for that measure, and outlines the measure while the window is open. Making another instrument active closes it, and so does deleting it or an instrument before it. For a chord that's already there, the window starts with its settings.

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
   - **Block**: a chord held for its note length, the whole measure to start with, written at its start so it lines up with a quarter note on the first beat of the other staff. With a shorter note length it's played again until the measure's full, e.g. on every beat in quarter notes, with a rest for anything left over.
   - **Arpeggio (asc)**: single notes going up through the chord to the first note an octave higher, unless the chord already reaches that far, and back down, over and over until the measure's full, every note the chord's note length: in eighths in 4/4, C E G C' G E C E. Notes are beamed like notes clicked in (four eighths in a row together).
   - **Arpeggio (desc)**: back and forth between the same notes, turning at the C at the bottom and the C an octave up, but starting on the chord's highest note and going down first, so it carries on from an arpeggio going up in the measure before: G E C E G C' G E. A chord that reaches past the octave, such as a 9th chord, turns at its own top and bottom notes instead.
   - **Random**: the notes in a random order, a note of the note length after another, without playing any note twice in a row, and every note at least once if there's room. **Reshuffle** picks a new order.
   - **Rolled chord**: a block chord with the wavy roll sign.
9. **Note length**, under the chord type, is how long the chord's notes are: **Whole measure** (a whole note in 4/4, a dotted half in 3/4 and a half in 2/4), half, quarter, eighth, 16th or 32nd notes, but not dotted ones; lengths longer than the measure can't be chosen. A block or rolled chord starts out lasting the measure; arpeggios and Random chords start out in quarter notes, or eighths or 16ths if there are too many notes for quarters to fit, and follow the chord's notes as they change until a length is chosen.

Each chord keeps its own clef, chord type and note length. A new chord starts with the ones chosen last. A chord replaces any quarter notes on its staff.

Under the settings, the window lists the chord's name and position, its notes from low to high, and whether they're in the key.

**On the piano:** while the chord window is open, clicking a key adds its note to the window's chord, or takes it out. The chord's notes are marked on the keys. The notes are named as a note, an interval, or a chord (including sus2, sus4, ø7, °7 and m(maj7)), and spelled to suit it, e.g. A♭ rather than G♯ in B°7. When the window can describe the notes, it fills in to match. Notes set on the piano keep their pitches if you change the key.

### What the staff shows

- Chord names above each chord, e.g. Am, G7, B♭maj9, B°, E+, and slash chords for inversions such as G/B.
- Roman numerals below, with their figures: I⁶, I⁶₄, V⁷, V⁶₅, V⁴₃, V⁴₂, V⁹, and an M before a major 7th's figures (I^M7).
- Accidentals only where a note differs from the key signature or from an earlier accidental in the measure.

### Playing

Every note is held for exactly as long as it's written on the staff, whatever wrote it: a quarter note for a beat, a half note for two, a whole note for four, a dotted quarter for a beat and a half, an eighth for half of one, a dotted eighth for three quarters of one, a 16th for a quarter of one, a 32nd for an eighth of one, with the next note starting as it ends. Block chords sound together and rolled chords bottom to top in quick succession; both are written as notes as long as the measure (a whole note in 4/4, a dotted half in 3/4), so that's how long they're held. Arpeggio and Random notes come one per written note, each for its own length. Alternate-staff notes start on the downbeat and are held for their length too. The only note cut shorter is one whose pitch starts again while it's still sounding, as a key can't be pressed twice at once.

Click or drag across the piano keys to play the active instrument yourself. Once the piano has been clicked, the computer keyboard plays it too: A is middle C, and the row from A to L (with W, E, T, Y, U, O and P for the black keys) plays upwards from there.

### MIDI controllers

Plug a MIDI controller into a USB port and it plays the active instrument like the on-screen piano, whose keys go down as you play. It's used as soon as it's plugged in, even with the app already running. The keys play at the velocity you strike them, and the sustain pedal, pitch bend, mod wheel and other controls go through to the plugin too. While the chord window is open, a key adds its note to the window's chord or takes it out, as clicking the piano does.

The **MIDI** menu lists the MIDI inputs, ticked when they're on. Choose one to turn it off or on; one that's off stays off the next time the app starts.

## Source

| File | Contents |
| --- | --- |
| `Source/Main.cpp` | The application and its window, and asking to save before quitting |
| `Source/MainComponent.*` | The File, Edit and MIDI menus, the toolbar, the sidebar, the instruments' scrolling staves, with their volume dials and delete buttons, adding and deleting instruments, the piano, and the audio output |
| `Source/Score.*` | The music: the key, time signature and speed, and each instrument's notes, by eighth note, dynamics, crescendos and decrescendos, chords and alternate staff (a part, in the code), and saving it as JSON |
| `Source/ScoreDocument.*` | Saves the score to a file and opens it, and keeps track of unsaved changes |
| `Source/ScoreHistory.*` | Remembers the score before each change, or each eraser stroke, for Undo and Redo |
| `Source/Music.*` | Music theory: keys, spelling, building, recognising and naming chords, and dynamics, hairpins and their velocities |
| `Source/MeasureContent.*` | Works out the notes, rests and their lengths that each measure has on each staff |
| `Source/StaffView.*` | Draws an instrument's grand staff with each measure's chord button, lined up with the other instruments', and turns clicks and drags into notes, dynamics and hairpins, or erases them |
| `Source/MusicGlyphs.*` | Music symbols from the Bravura font |
| `Source/ScorePanel.*` | The key, time signature and measures settings |
| `Source/ChordEditor.*` | The chord window, with the settings for a measure's chord |
| `Source/DynamicPicker.*` | The ppp to fff, crescendo and decrescendo buttons for choosing what clicks and drags mark |
| `Source/EraserButton.*` | The eraser button, and the eraser the pointer becomes |
| `Source/NoteLengthPicker.*` | The whole, dotted half, half, dotted quarter, quarter, dotted eighth, eighth, 16th and 32nd note buttons for choosing the length of the notes clicks add |
| `Source/PianoKeyboard.*` | The on-screen piano |
| `Source/Controls.h` | Colours and helpers shared by the controls |
| `Source/InstrumentHost.*` | Hosts each instrument's plugin, and plays them from the piano, MIDI controllers and the score, once or looping |
| `Source/MidiInputs.*` | Connects MIDI controllers, as they're plugged in, and the MIDI menu's settings |
| `Source/InstrumentPanel.*` | Loads an instrument's plugin and opens its editor, and saves and loads it with the score |
| `Source/PluginWindow.*` | The window for an instrument's editor |
| `Resources/Icon.png` | The app's icon, a staff with two quarter notes, from which the build makes the Mac icon |

## Licences

JUCE is dual-licensed under the AGPLv3 and the commercial JUCE licence. The Bravura music font is © Steinberg Media Technologies GmbH and licensed under the SIL Open Font License 1.1; see `Resources/Fonts/Bravura-LICENSE.txt`.
