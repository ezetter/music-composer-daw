# Anthropocene Music

A native macOS music app written in C++ with [JUCE](https://juce.com).

So far it has a grand staff you can write quarter notes on, and an 88-key piano you can play with the mouse. The piano makes no sound yet. JUCE's VST3 and Audio Unit hosting are compiled in, and JUCE handles MIDI input, ready for plugins and MIDI devices.

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
- Click or drag across the piano keys to press them.

## Source

| File | Contents |
| --- | --- |
| `Source/Main.cpp` | The application and its window |
| `Source/MainComponent.*` | The toolbar, the scrolling staff, and the piano |
| `Source/Score.*` | The music: measures of quarter-note chords on the two staves |
| `Source/StaffView.*` | Draws the grand staff and turns clicks into notes |
| `Source/MusicGlyphs.*` | Music symbols from the Bravura font |

## Licences

JUCE is dual-licensed under the AGPLv3 and the commercial JUCE licence. The Bravura music font is © Steinberg Media Technologies GmbH and licensed under the SIL Open Font License 1.1; see `Resources/Fonts/Bravura-LICENSE.txt`.
