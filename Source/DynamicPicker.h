#pragma once

#include "Music.h"
#include "MusicGlyphs.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <optional>

/** Small buttons, side by side, showing the dynamics from ppp to fff, for choosing one for clicks
    on the score to mark. Clicking the chosen one again goes back to none.
*/
class DynamicPicker final : public juce::Component
{
public:
    DynamicPicker();

    /** The dynamic chosen, if any. */
    std::optional<music::Dynamic> getDynamic() const noexcept { return dynamic; }

    /** Chooses a dynamic, or none, without calling onChange. */
    void setDynamic (std::optional<music::Dynamic>);

    /** Called when a button's clicked, with the dynamic chosen now, or none. */
    std::function<void (std::optional<music::Dynamic>)> onChange;

    /** The size the buttons need, side by side. */
    juce::Rectangle<int> getIdealBounds() const;

    void resized() override;

    static constexpr int buttonWidth = 36, buttonHeight = 28;      // wide enough for ppp and fff

private:
    /** A button showing a dynamic. */
    struct DynamicButton final : public juce::Button
    {
        DynamicButton (const MusicGlyphs&, music::Dynamic);

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

        const MusicGlyphs& glyphs;
        const music::Dynamic dynamic;
    };

    MusicGlyphs glyphs;
    std::array<std::unique_ptr<DynamicButton>, music::allDynamics.size()> buttons;
    std::optional<music::Dynamic> dynamic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicPicker)
};
