# Anthropocene Music

A native macOS music app written in C++ with [JUCE](https://juce.com).

So far it has a grand staff you can write quarter notes on, an 88-key piano, and a VST3 instrument that plays both of them. Sound goes to the Mac's default audio output.

## Building

You need macOS 12 or later, the Xcode Command Line Tools, and CMake 3.22 or later (`brew install cmake`).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
open "build/AnthropoceneMusic_artefacts/Debug/Anthropocene Music.app"
```

The first configure downloads JUCE 9.0.2 into `build/`.

## Using it

- Click the staff to add a quarter note at that pitch and beat. A blue note shows where it will go, and clicking between the staves puts the note on the nearer staff, with ledger lines.
- **Add Measure** and **Remove Measure** change the length of the score. Scroll sideways with a trackpad, the mouse wheel, or the scroll bar.
- **Load Instrument…** loads a VST3 instrument, such as one from `/Library/Audio/Plug-Ins/VST3`. **Show Editor** opens the instrument's own window, where you can choose its sounds.
- Click or drag across the piano keys to play the instrument. Once the piano has been clicked, the computer keyboard plays it too: A is middle C, and the row from A to L (with W, E, T, Y, U, O and P for the black keys) plays upwards from there.
- **Play** (or the space bar) plays the score from the beginning at 120 beats per minute, highlighting each beat and following it along the staff. **Stop** stops it.

## Source

| File | Contents |
| --- | --- |
| `Source/Main.cpp` | The application and its window |
| `Source/MainComponent.*` | The toolbar, the scrolling staff, the piano, and the audio output |
| `Source/Score.*` | The music: measures of quarter-note chords on the two staves |
| `Source/StaffView.*` | Draws the grand staff and turns clicks into notes |
| `Source/MusicGlyphs.*` | Music symbols from the Bravura font |
| `Source/InstrumentHost.*` | Hosts the instrument plugin, and plays it from the piano and the score |
| `Source/InstrumentPanel.*` | Loads instruments and opens their editors |
| `Source/PluginWindow.*` | The window for an instrument's editor |

## Licences

JUCE is dual-licensed under the AGPLv3 and the commercial JUCE licence. The Bravura music font is © Steinberg Media Technologies GmbH and licensed under the SIL Open Font License 1.1; see `Resources/Fonts/Bravura-LICENSE.txt`.
