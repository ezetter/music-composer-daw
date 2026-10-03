#pragma once

#include "MusicGlyphs.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

/** Small buttons, side by side, showing a whole, a dotted half, a half, a quarter, an eighth, a
    16th and a 32nd note, for choosing how long the notes that clicks add are. One is chosen at a time.
*/
class NoteLengthPicker final : public juce::Component
{
public:
    NoteLengthPicker();

    /** The length chosen, in beats: 4, 3, 2, 1, 0.5, 0.25 or 0.125. */
    double getLength() const noexcept { return length; }

    /** Chooses a length, in beats, without calling onChange. */
    void setLength (double beats);

    /** Shows which length is chosen, or none while clicks are doing something else, such as
        marking dynamics. Clicking a length shows it again, and calls onChange.
    */
    void setChoiceShown (bool);

    /** Called when a button's clicked, with the length it chooses. */
    std::function<void (double beats)> onChange;

    /** The size the buttons need, side by side. */
    juce::Rectangle<int> getIdealBounds() const;

    void resized() override;

    static constexpr int buttonSize = 28;

private:
    /** A button showing a note. */
    struct NoteButton final : public juce::Button
    {
        NoteButton (const MusicGlyphs&, double beats);

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

        const MusicGlyphs& glyphs;
        const double beats;
    };

    MusicGlyphs glyphs;
    std::array<std::unique_ptr<NoteButton>, 7> buttons;
    double length = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteLengthPicker)
};
