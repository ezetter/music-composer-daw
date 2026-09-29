#pragma once

#include "MusicGlyphs.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

/** Small buttons, side by side, showing a whole, a half and a quarter note, for choosing how
    long the notes that clicks add are. One is chosen at a time.
*/
class NoteLengthPicker final : public juce::Component
{
public:
    NoteLengthPicker();

    /** The length chosen, in beats: 4, 2 or 1. */
    int getLength() const noexcept { return length; }

    /** Chooses a length, in beats, without calling onChange. */
    void setLength (int beats);

    /** Called when a button's clicked, with the length it chooses. */
    std::function<void (int beats)> onChange;

    /** The size the buttons need, side by side. */
    juce::Rectangle<int> getIdealBounds() const;

    void resized() override;

    static constexpr int buttonSize = 28;

private:
    /** A button showing a note. */
    struct NoteButton final : public juce::Button
    {
        NoteButton (const MusicGlyphs&, int beats);

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

        const MusicGlyphs& glyphs;
        const int beats;
    };

    MusicGlyphs glyphs;
    std::array<std::unique_ptr<NoteButton>, 3> buttons;
    int length = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteLengthPicker)
};
