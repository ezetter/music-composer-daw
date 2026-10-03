#include "DynamicPicker.h"

#include "Controls.h"

namespace
{
    constexpr float glyphStaffSpace = 6.5f;     // the dynamics are drawn as if on a staff this size
    constexpr int gap = 3;

    juce::String describe (music::Marking marking)
    {
        if (const auto* dynamic = std::get_if<music::Dynamic> (&marking))
            return music::getDynamicMark (*dynamic) + " (" + music::getDynamicName (*dynamic) + "): click the score to mark it, "
                   "and the notes from there on play at velocity " + juce::String (music::getDynamicVelocity (*dynamic));

        if (std::holds_alternative<music::Pedal> (marking))
            return "Pedal: click under an instrument's lower staff to put the sustain pedal down, and again to lift it. "
                   "Clicking between where it goes down and comes up lifts it and puts it straight down again there. "
                   "Notes are held while it's down.";

        const auto hairpin = std::get<music::Hairpin> (marking);
        return music::getHairpinName (hairpin) + ": drag along the score to mark one, or drag the end of one to stretch it. "
               "Each beat, the notes get " + juce::String (juce::roundToInt (music::hairpinChangePerBeat * 100.0)) + "% of the way "
               + (hairpin == music::Hairpin::crescendo ? "to the loudest" : "to the softest");
    }
}

DynamicPicker::DynamicPicker()
    : glyphs (glyphStaffSpace)
{
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        const auto numDynamics = music::allDynamics.size();
        const auto shown = i < numDynamics ? music::Marking (music::allDynamics[i])
                         : i == numDynamics ? music::Marking (music::Hairpin::crescendo)
                         : i == numDynamics + 1 ? music::Marking (music::Hairpin::decrescendo)
                                                : music::Marking (music::Pedal::sustain);
        auto& button = buttons[i];
        button = std::make_unique<MarkingButton> (glyphs, shown);
        button->setTooltip (describe (shown));
        button->setWantsKeyboardFocus (false);
        button->onClick = [this, shown]
        {
            setChoice (choice == shown ? std::nullopt : std::optional (shown));

            if (onChange != nullptr)
                onChange (choice);
        };
        addAndMakeVisible (*button);
    }

    setSize (getIdealBounds().getWidth(), getIdealBounds().getHeight());
}

void DynamicPicker::setChoice (std::optional<music::Marking> newChoice)
{
    choice = newChoice;

    for (auto& button : buttons)
        button->setToggleState (button->marking == choice, juce::dontSendNotification);
}

juce::Rectangle<int> DynamicPicker::getIdealBounds() const
{
    return { (int) buttons.size() * buttonWidth + ((int) buttons.size() - 1) * gap + 2 * groupGap, buttonHeight };
}

void DynamicPicker::resized()
{
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds ((int) i * (buttonWidth + gap) + (i >= music::allDynamics.size() ? groupGap : 0)
                                   + (i >= music::allDynamics.size() + 2 ? groupGap : 0),
                               0, buttonWidth, buttonHeight);
}

//==============================================================================
DynamicPicker::MarkingButton::MarkingButton (const MusicGlyphs& glyphsToUse, music::Marking markingToShow)
    : juce::Button ({}),
      glyphs (glyphsToUse),
      marking (markingToShow)
{
}

void DynamicPicker::MarkingButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto ink = controls::drawSymbolButtonTile (g, *this, highlighted, down);
    const auto centre = getLocalBounds().toFloat().getCentre();
    g.setColour (ink);

    // A dynamic's letters centred across the button, and their middles halfway down
    if (const auto* dynamic = std::get_if<music::Dynamic> (&marking))
    {
        const auto glyph = Smufl::dynamics[(size_t) *dynamic];
        const auto glyphBounds = glyphs.getPath (glyph).getBounds();
        glyphs.draw (g, glyph, { centre.x - glyphBounds.getCentreX(), centre.y + 0.45f * glyphStaffSpace });
        return;
    }

    // The pedal: a line with ticks up at each end, where it goes down and comes up
    if (std::holds_alternative<music::Pedal> (marking))
    {
        const auto area = getLocalBounds().toFloat().withSizeKeepingCentre (0.64f * (float) getWidth(), 0.36f * (float) getHeight());
        juce::Path bracket;
        bracket.startNewSubPath (area.getTopLeft());
        bracket.lineTo (area.getBottomLeft());
        bracket.lineTo (area.getBottomRight());
        bracket.lineTo (area.getTopRight());
        g.strokePath (bracket, juce::PathStrokeType (1.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
        return;
    }

    // A hairpin: two lines meeting at one end and opening out at the other
    const auto crescendo = std::get<music::Hairpin> (marking) == music::Hairpin::crescendo;
    const auto halfWidth = 0.32f * (float) getWidth(), halfOpening = 0.2f * (float) getHeight();
    const auto point = juce::Point<float> (crescendo ? centre.x - halfWidth : centre.x + halfWidth, centre.y);
    const auto openX = crescendo ? centre.x + halfWidth : centre.x - halfWidth;

    juce::Path wedge;
    wedge.startNewSubPath (openX, centre.y - halfOpening);
    wedge.lineTo (point);
    wedge.lineTo (openX, centre.y + halfOpening);
    g.strokePath (wedge, juce::PathStrokeType (1.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
}
