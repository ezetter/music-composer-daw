#pragma once

#include "Music.h"
#include "MusicGlyphs.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <optional>

/** Small buttons, side by side, showing the dynamics from ppp to fff, then a crescendo and a
    decrescendo hairpin, then the sustain pedal, then start and end repeat signs, for choosing one
    for the score to mark. Clicking the chosen one again goes back to none.
*/
class DynamicPicker final : public juce::Component
{
public:
    DynamicPicker();

    /** The dynamic or hairpin chosen, if any. */
    std::optional<music::Marking> getChoice() const noexcept { return choice; }

    /** Chooses a dynamic or hairpin, or none, without calling onChange. */
    void setChoice (std::optional<music::Marking>);

    /** Called when a button's clicked, with what's chosen now, or none. */
    std::function<void (std::optional<music::Marking>)> onChange;

    /** The size the buttons need, side by side. */
    juce::Rectangle<int> getIdealBounds() const;

    void resized() override;

    static constexpr int buttonWidth = 36, buttonHeight = 28;      // wide enough for ppp and fff
    static constexpr int groupGap = 8;                              // more space between the dynamics, the hairpins, the pedal and the repeat signs

private:
    /** A button showing a dynamic or a hairpin. */
    struct MarkingButton final : public juce::Button
    {
        MarkingButton (const MusicGlyphs&, music::Marking);

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

        const MusicGlyphs& glyphs;
        const music::Marking marking;
    };

    MusicGlyphs glyphs;
    std::array<std::unique_ptr<MarkingButton>, music::allDynamics.size() + 5> buttons;
    std::optional<music::Marking> choice;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicPicker)
};
