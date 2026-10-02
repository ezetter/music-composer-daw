#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** A small button showing an eraser, for choosing to erase what's on the score: while it's on,
    whatever the mouse passes over with its button held down is taken out.
*/
class EraserButton final : public juce::Button
{
public:
    EraserButton();

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    /** The mouse pointer while erasing: an eraser, rubbing with the corner of its tip. */
    static juce::MouseCursor createCursor();

    static constexpr int buttonWidth = 36, buttonHeight = 28;

private:
    /** An eraser lying at a slant in an area, its tip at the bottom left: the outline of all of it,
        and the tip, which is filled in.
    */
    static std::pair<juce::Path, juce::Path> getEraserShape (juce::Rectangle<float>);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EraserButton)
};
