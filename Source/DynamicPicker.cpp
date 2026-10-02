#include "DynamicPicker.h"

#include "Controls.h"

namespace
{
    constexpr float glyphStaffSpace = 6.5f;     // the dynamics are drawn as if on a staff this size
    constexpr int gap = 3;
}

DynamicPicker::DynamicPicker()
    : glyphs (glyphStaffSpace)
{
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        const auto shown = music::allDynamics[i];
        auto& button = buttons[i];
        button = std::make_unique<DynamicButton> (glyphs, shown);
        button->setTooltip (music::getDynamicMark (shown) + " (" + music::getDynamicName (shown) + "): click the score to mark it, "
                            "and the notes from there on play at velocity " + juce::String (music::getDynamicVelocity (shown)));
        button->setWantsKeyboardFocus (false);
        button->onClick = [this, shown]
        {
            setDynamic (dynamic == shown ? std::nullopt : std::optional (shown));

            if (onChange != nullptr)
                onChange (dynamic);
        };
        addAndMakeVisible (*button);
    }

    setSize (getIdealBounds().getWidth(), getIdealBounds().getHeight());
}

void DynamicPicker::setDynamic (std::optional<music::Dynamic> newDynamic)
{
    dynamic = newDynamic;

    for (auto& button : buttons)
        button->setToggleState (button->dynamic == dynamic, juce::dontSendNotification);
}

juce::Rectangle<int> DynamicPicker::getIdealBounds() const
{
    return { (int) buttons.size() * buttonWidth + ((int) buttons.size() - 1) * gap, buttonHeight };
}

void DynamicPicker::resized()
{
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds ((int) i * (buttonWidth + gap), 0, buttonWidth, buttonHeight);
}

//==============================================================================
DynamicPicker::DynamicButton::DynamicButton (const MusicGlyphs& glyphsToUse, music::Dynamic dynamicToShow)
    : juce::Button ({}),
      glyphs (glyphsToUse),
      dynamic (dynamicToShow)
{
}

void DynamicPicker::DynamicButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto ink = controls::drawSymbolButtonTile (g, *this, highlighted, down);

    // The letters centred across the button, and their middles, on the baseline, halfway down
    const auto glyph = Smufl::dynamics[(size_t) dynamic];
    const auto glyphBounds = glyphs.getPath (glyph).getBounds();
    const auto centre = getLocalBounds().toFloat().getCentre();

    g.setColour (ink);
    glyphs.draw (g, glyph, { centre.x - glyphBounds.getCentreX(), centre.y + 0.45f * glyphStaffSpace });
}
